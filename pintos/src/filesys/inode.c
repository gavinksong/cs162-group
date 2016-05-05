#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/buffer-cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define NUM_DIRECT 119
#define NUM_INDIRECT 128
#define MAX_LENGTH 8388608

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    /* Data blocks. */
    block_sector_t direct[NUM_DIRECT];    /* Direct pointers. */
    block_sector_t indirect;              /* Indirect pointer. */
    block_sector_t doubly_indirect;       /* Doubly indirect pointer. */

    /* Filesys metadata. */
    block_sector_t parent;                /* inode_disk sector of the parent directory. */
    off_t ofs;                            /* Offset of entry in parent directory. */
    bool isdir;                           /* True if this file is a directory. */
    uint32_t num_files;                   /* The number of subdirectories or files. */

    /* Misc. */
    off_t length;                         /* File size in bytes. */
    unsigned magic;                       /* Note: magic has a different offset now. */
    uint8_t unused[3];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  };

/* The following functions are meant to be passed as arguments
   to the function inode_map_sectors (), defined further below. */

typedef bool inode_map_func (size_t start, block_sector_t *sectors,
                             size_t cnt, void *aux);

static bool allocate_sectors (size_t start, block_sector_t *sectors,
                              size_t cnt, void *aux UNUSED);

static bool deallocate_sectors (size_t start, block_sector_t *sectors,
                                size_t cnt, void *aux UNUSED);

static bool write_to_sectors (size_t start, block_sector_t *sectors,
                              size_t cnt, void *aux);

static bool read_from_sectors (size_t start, block_sector_t *sectors,
                               size_t cnt, void *aux);

/* Applies MAP_FUNC on arrays of sector numbers for all of
   INODE's data blocks indexed between START (inclusive) and
   END (exclusive) in order. The arrays are passed by reference.
   Assumes that all indirect and doubly indirect pointers in
   the inode are valid. */
static bool
inode_map_sectors (const struct inode_disk *inode,
                   inode_map_func *map_func,
                   size_t start, size_t end,
                   void *aux)
{
  ASSERT (inode != NULL);
  ASSERT (end <= bytes_to_sectors (MAX_LENGTH));

  size_t table_start = 0;
  block_sector_t *sectors;
  block_sector_t *indirects;

/* Applies MAP_FUNC to the portion of SECTORS between the START and
   END indices, and advances START by the number of sectors mapped. */
#define apply(num_pointers) {                                 \
  size_t table_end = num_pointers + table_start;              \
  size_t cnt = (end < table_end ? end : table_end) - start;   \
  if (!map_func (start, &sectors[start - table_start],        \
                 cnt, aux))                                   \
    return false;                                             \
  table_start = table_end;                                    \
  start += cnt;                                               \
}

  /* Apply to direct blocks. */
  if (start < NUM_DIRECT) {
    sectors = (block_sector_t *) inode->direct;
    apply (NUM_DIRECT);
  }
  if (end <= start)
    return true;

  /* Apply to indirect blocks. */
  table_start = NUM_DIRECT;
  if (start < NUM_DIRECT + NUM_INDIRECT) {
    sectors = buffer_cache_get (inode->indirect);
    apply (NUM_INDIRECT);
    buffer_cache_release (sectors, true);
  }
  if (end <= start)
    return true;

  /* Apply to doubly indirect blocks. */
  size_t i = (start - NUM_DIRECT) / NUM_INDIRECT - 1;
  table_start = NUM_DIRECT + (i + 1) * NUM_INDIRECT;
  indirects = buffer_cache_get (inode->doubly_indirect);
  while (start < end) {
    sectors = buffer_cache_get (indirects[i++]);
    apply (NUM_INDIRECT);
    buffer_cache_release (sectors, true);
  }
  buffer_cache_release (indirects, false);

#undef apply
  return true;
}

/* Shortens the length of INODE to LENGTH, deallocating sectors
   as necessary. */
static void
shorten_inode_length (struct inode_disk *inode, off_t length)
{
  ASSERT (inode != NULL);
  ASSERT (length <= inode->length);

  size_t start = bytes_to_sectors (length);
  size_t end = bytes_to_sectors (inode->length);
  size_t border = NUM_DIRECT;

  /* Free leaf nodes. */
  inode_map_sectors (inode, deallocate_sectors, start, end, NULL);

  /* Free INDIRECT. */
  if (start <= border && border < end)
    free_map_release (inode->indirect, 1);

  border += NUM_INDIRECT;

  /* Release all pointers in DOUBLY_INDIRECT sector. */
  if (border < end) {
    size_t i = (start > border) ? DIV_ROUND_UP (start - border, NUM_INDIRECT) : 0;
    size_t cnt = DIV_ROUND_UP (end - border,  NUM_INDIRECT) - i;
    block_sector_t *indirects = buffer_cache_get (inode->doubly_indirect);
    free_map_release_nc (&indirects[i], cnt);
    buffer_cache_release (indirects, true);
  }

  /* Free DOUBLY_INDIRECT. */
  if (start <= border && border < end)
    free_map_release (inode->doubly_indirect, 1);

  inode->length = length;
}

/* Extends the length of INODE to the LENGTH, allocating new
   sectors as needed. */
static bool
extend_inode_length (struct inode_disk *inode, off_t length)
{
  ASSERT (inode != NULL);
  ASSERT (length <= MAX_LENGTH);
  ASSERT (length >= inode->length);

  size_t start = bytes_to_sectors (inode->length);
  size_t end = bytes_to_sectors (length);
  size_t border = NUM_DIRECT;

  /* Acquire free map lock and check available space. */
  lock_acquire (&free_map_lock);
  if (free_map_available_space () < end - start
                                    + (end / NUM_DIRECT)
                                    - (start / NUM_DIRECT)) {
    lock_release (&free_map_lock);
    return false;
  }

  /* Allocate INDIRECT. */
  if (start <= border && border < end)
    free_map_allocate (1, &inode->indirect);

  border += NUM_INDIRECT;

  /* Allocate DOUBLY_INDIRECT. */
  if (start <= border && border < end)
    free_map_allocate (1, &inode->doubly_indirect);

  /* Set all pointers in DOUBLY_INDIRECT. */
  if (border < end) {
    size_t i = (start > border) ? DIV_ROUND_UP (start - border , NUM_INDIRECT) : 0;
    size_t cnt = DIV_ROUND_UP (end - border, NUM_INDIRECT) - i;
    block_sector_t *indirects = buffer_cache_get (inode->doubly_indirect);
    free_map_allocate_nc (cnt, &indirects[i]);
    buffer_cache_release (indirects, true);
  }

  /* Allocate all leaf nodes and set INODE's LENGTH. */
  inode_map_sectors (inode, allocate_sectors, start, end, NULL);
  lock_release (&free_map_lock);
  inode->length = length;
  return true;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
  thread_current()->cwd = inode_open(ROOT_DIR_SECTOR);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = buffer_cache_get (sector);
  disk_inode->parent = sector;
  disk_inode->length = 0;
  disk_inode->isdir = isdir;
  disk_inode->num_files = 0;
  disk_inode->magic = INODE_MAGIC;
  success = extend_inode_length (disk_inode, length);
  buffer_cache_release (disk_inode, true);
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          struct inode_disk *data = buffer_cache_get (inode->sector);
          shorten_inode_length (data, 0);
          buffer_cache_release (data, true);
          free_map_release (inode->sector, 1);
        }

      free (inode);
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Auxilary data struct. */
struct buffer_aux {
  void *buffer;
  off_t size;
  off_t pos;
  off_t offset;
};

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  struct inode_disk *disk_inode = buffer_cache_get (inode->sector);
  if (disk_inode->length < offset)
    return 0;
  /* Read up until the end-of-file. */
  if (disk_inode->length < offset + size)
    size = disk_inode->length - offset;

  size_t start = offset / BLOCK_SECTOR_SIZE;
  size_t end = DIV_ROUND_UP (offset + size, BLOCK_SECTOR_SIZE);
  struct buffer_aux *aux = malloc (sizeof (struct buffer_aux));
  aux->buffer = (void *) buffer_;
  aux->size = size;
  aux->offset = offset;
  aux->pos = 0;

  inode_map_sectors (disk_inode, read_from_sectors, start, end, aux);
  buffer_cache_release (disk_inode, false);
  free (aux);

  return size;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if an error occurs. */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  if (inode->deny_write_cnt)
    return 0;

  struct inode_disk *disk_inode = buffer_cache_get (inode->sector);
  if (disk_inode->length < offset + size)
    /* Quit if there isn't enough space on disk. */
    if (!extend_inode_length (disk_inode, offset + size))
      return 0;

  size_t start = offset / BLOCK_SECTOR_SIZE;
  size_t end = DIV_ROUND_UP (offset + size, BLOCK_SECTOR_SIZE);
  struct buffer_aux *aux = malloc (sizeof (struct buffer_aux));
  aux->buffer = (void *) buffer_;
  aux->size = size;
  aux->offset = offset;
  aux->pos = 0;

  inode_map_sectors (disk_inode, write_to_sectors, start, end, aux);
  buffer_cache_release (disk_inode, true);
  free (aux);

  return size;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  struct inode_disk *data = buffer_cache_get (inode->sector);
  off_t length = data->length;
  buffer_cache_release (data, false);
  return length;
}

/* Returns true if INODE is a directory, false otherwise. */
bool
inode_isdir (const struct inode *inode)
{
  struct inode_disk *disk_inode = buffer_cache_get (inode->sector);
  bool isdir = disk_inode->isdir;
  buffer_cache_release (disk_inode, false);
  return isdir;
}

/* Opens INODE's parent directory inode. */
struct inode *
inode_open_parent (struct inode *inode)
{
  if (inode != NULL) {
    struct inode_disk *disk_inode = buffer_cache_get (inode->sector);
    block_sector_t parent = disk_inode->parent;
    buffer_cache_release (disk_inode, false);
    inode = inode_open (parent);
  }
  return inode;
}

/* Returns the offset of INODE's entry in INODE's parent directory. */
off_t
inode_offset (const struct inode *inode) {
  struct inode_disk *disk_inode = buffer_cache_get (inode->sector);
  off_t ofs = disk_inode->ofs;
  buffer_cache_release (disk_inode, false);
  return ofs;
}

/* Returns the number of subdirectories or files in INODE. */
uint32_t
inode_num_files (const struct inode *inode)
{
  struct inode_disk *disk_inode = buffer_cache_get (inode->sector);
  uint32_t num_files = disk_inode->num_files;
  buffer_cache_release (disk_inode, false);
  return num_files;
}

/* If PARENT is a directory, sets the parent and ofs
   members of the inode in CHILD SECTOR to PARENT and
   OFS. Increments the num_files of PARENT by one. */
bool
inode_add_file (const struct inode *parent, block_sector_t child_sector, off_t ofs)
{
  struct inode_disk *disk_inode;

  if (!inode_isdir (parent))
    return false;

  disk_inode = buffer_cache_get (child_sector);
  disk_inode->parent = parent->sector;
  disk_inode->ofs = ofs;
  buffer_cache_release (disk_inode, true);

  disk_inode = buffer_cache_get(parent->sector);
  disk_inode->num_files += 1;
  buffer_cache_release (disk_inode, true);

  return true;
}

/* Decrement num_files of INODE. */
bool
inode_remove_file (const struct inode *inode)
{
  if (!inode_isdir (inode))
    return false;

  struct inode_disk *disk_inode = buffer_cache_get(inode->sector);
  disk_inode->num_files -= 1;
  buffer_cache_release (disk_inode, true);
  return true;
}

int
get_open_cnt (const struct inode *inode) {
  return inode->open_cnt;
}

/* Allocates and zeros-out CNT new sectors.
   Stores the sector numbers in SECTORS. */
static bool
allocate_sectors (size_t start UNUSED, block_sector_t *sectors,
                  size_t cnt, void *aux UNUSED)
{
  bool success = free_map_allocate_nc (cnt, sectors);
  if (success) {
    void *zeros = calloc (BLOCK_SECTOR_SIZE, 1);
    size_t i = 0;
    while (i < cnt)
      buffer_cache_write (sectors[i++], zeros);
    free (zeros);
  }
  return success;
}

/* Frees up the first CNT sectors in SECTORS. */
static bool
deallocate_sectors (size_t start UNUSED, block_sector_t *sectors,
                    size_t cnt, void *aux UNUSED)
{
  free_map_release_nc (sectors, cnt);
  return true;
}

/* For your reference:

    struct buffer_aux {
      void *buffer;
      off_t size;
      off_t pos;
      off_t offset;
    }
*/

static bool
write_to_sectors (size_t start, block_sector_t *sectors,
                  size_t cnt, void *aux_)
{
  struct buffer_aux *aux = aux_;

  /* Index of the starting data block. */
  size_t block_idx = aux->offset / BLOCK_SECTOR_SIZE;
  ASSERT (block_idx >= start || block_idx < start + cnt);

  size_t i;
  for (i = block_idx - start; i < cnt; i++) {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector = sectors[i];
    int sector_ofs = aux->offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in sector, bytes left in buffer. */
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int buffer_left = aux->size - aux->pos;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = buffer_left < sector_left ? buffer_left : sector_left;
    if (chunk_size <= 0)
      break;

    /* Load sector into cache, then partially copy into caller's buffer. */
    void *cache_block = buffer_cache_get (sector);
    memcpy (cache_block + sector_ofs, aux->buffer + aux->pos, chunk_size);
    buffer_cache_release (cache_block, true);

    /* Advance. */
    aux->offset += chunk_size;
    aux->pos += chunk_size;
  }

  return true;
}

static bool
read_from_sectors (size_t start, block_sector_t *sectors,
                   size_t cnt, void *aux_)
{
  struct buffer_aux *aux = aux_;

  /* Index of the starting data block. */
  size_t block_idx = aux->offset / BLOCK_SECTOR_SIZE;
  ASSERT (block_idx >= start || block_idx < start + cnt);

  size_t i;
  for (i = block_idx - start; i < cnt; i++) {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector = sectors[i];
    int sector_ofs = aux->offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in sector, bytes left in buffer. */
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int buffer_left = aux->size - aux->pos;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = buffer_left < sector_left ? buffer_left : sector_left;
    if (chunk_size <= 0)
      break;

    /* Load sector into cache, then partially copy into caller's buffer. */
    void *cache_block = buffer_cache_get (sector);
    memcpy (aux->buffer + aux->pos, cache_block + sector_ofs, chunk_size);
    buffer_cache_release (cache_block, false);

    /* Advance. */
    aux->offset += chunk_size;
    aux->pos += chunk_size;
  }

  return true;
}
