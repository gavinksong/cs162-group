#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/buffer-cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define NUM_DIRECT 122
#define NUM_INDIRECT 128
#define MAX_LENGTH 8388608

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct[NUM_DIRECT];    /* Direct pointers. */
    block_sector_t indirect;              /* Indirect pointer. */
    block_sector_t doubly_indirect;       /* Doubly indirect pointer. */
    block_sector_t parent_dir;            /* inode_disk sector of the parent directory. */
    bool is_dir;                          /* True if this file is a directory. */
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

/* Shortens the length of INODE to LENGTH, deallocating sectors
   as necessary. */
static void
shorten_inode_length (struct inode_disk *inode, off_t length)
{
  ASSERT (inode != NULL);
  ASSERT (length <= MAX_LENGTH);

  size_t start = bytes_to_sectors (inode->length);
  size_t end = bytes_to_sectors (length);
  bool do_release = false;

  block_sector_t *sectors;
  block_sector_t *indirects;

  if (start < NUM_DIRECT) {
    goto direct;
  }
  else if (start < NUM_DIRECT + NUM_INDIRECT) {
    start -= NUM_DIRECT;
    end -= NUM_DIRECT;
    goto indirect;
  }
  else {
    start -= NUM_DIRECT - NUM_INDIRECT;
    end -= NUM_DIRECT - NUM_INDIRECT;
    goto doubly_indirect;
  }

#define deallocate_blocks(num_pointers) {                     \
  size_t min_end = end < num_pointers ? end : num_pointers;   \
  size_t cnt = min_end - start;                               \
  free_map_release_nc (&sectors[start], cnt);                 \
  start = 0;                                                  \
  end -= num_pointers;                                        \
}

direct:
  sectors = (block_sector_t *) inode->direct;
  deallocate_blocks(NUM_DIRECT);
  if (end <= 0)
    goto done;

  do_release = true;

indirect:
  sectors = buffer_cache_get (inode->indirect);
  deallocate_blocks(NUM_INDIRECT);
  buffer_cache_release (sectors);
  if (do_release)
    free_map_release (inode->indirect, 1);
  if (end <= 0)
    goto done;

  do_release = true;

doubly_indirect:
  indirects = buffer_cache_get (inode->doubly_indirect);
  size_t i = start / NUM_INDIRECT;
  start = start % NUM_INDIRECT;
  for (; end > 0; i++) {
    bool dealloc_after = (start == 0);
    sectors = buffer_cache_get (indirects[i]);
    deallocate_blocks(NUM_INDIRECT);
    buffer_cache_release (sectors);
    if (dealloc_after)
      free_map_release (indirects[i], 1);
  }
  buffer_cache_release (indirects);
  if (do_release)
    free_map_release (inode->doubly_indirect, 1);

#undef deallocate_blocks

done:
  inode->length = length;
}

/* Extends the length of INODE to the LENGTH, allocating new
   sectors as needed. */
static bool
extend_inode_length (struct inode_disk *inode, off_t length)
{
  ASSERT (inode != NULL);
  ASSERT (length <= MAX_LENGTH);

  size_t original_length = length;
  size_t start = bytes_to_sectors (inode->length);
  size_t end = bytes_to_sectors (length);

  block_sector_t *sectors;
  block_sector_t *indirects;

  if (start < NUM_DIRECT) {
    goto direct;
  }
  else if (start < NUM_DIRECT + NUM_INDIRECT) {
    start -= NUM_DIRECT;
    end -= NUM_DIRECT;
    goto indirect;
  }
  else {
    start -= NUM_DIRECT - NUM_INDIRECT;
    end -= NUM_DIRECT - NUM_INDIRECT;
    goto doubly_indirect;
  }

#define allocate_blocks(num_pointers) {                       \
  size_t min_end = end < num_pointers ? end : num_pointers;   \
  size_t cnt = min_end - start;                               \
  if (!free_map_allocate_nc (cnt, &sectors[start]))           \
    goto fail;                                                \
  inode->length += cnt * BLOCK_SECTOR_SIZE;                   \
  start = 0;                                                  \
  end -= num_pointers;                                        \
}

direct:
  sectors = (block_sector_t *) inode->direct;
  allocate_blocks(NUM_DIRECT);
  if (end <= 0)
    goto done;

  free_map_allocate (1, &inode->indirect);

indirect:
  sectors = buffer_cache_get (inode->indirect);
  allocate_blocks(NUM_INDIRECT);
  buffer_cache_release (sectors); // may not be reached
  if (end <= 0)
    goto done;

  free_map_allocate (1, &inode->doubly_indirect);

doubly_indirect:
  indirects = buffer_cache_get (inode->doubly_indirect);
  size_t i = start / NUM_INDIRECT;
  start = start % NUM_INDIRECT;
  for (; end > 0; i++) {
    if (start == 0)
      free_map_allocate (1, &indirects[i]);
    sectors = buffer_cache_get (indirects[i]);
    allocate_blocks(NUM_INDIRECT);
    buffer_cache_release (sectors);
  }
  buffer_cache_release (indirects);

#undef allocate_blocks

done:
  inode->length = length;
  return true;

fail:
  shorten_inode_length (inode, original_length);
  return false;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);

  struct inode_disk *data = buffer_cache_get (inode->sector);
  block_sector_t *current = (block_sector_t *) data;
  block_sector_t retval = -1;
  
  if (pos < data->length) {
    size_t i = pos / BLOCK_SECTOR_SIZE;
  
    if (i >= NUM_DIRECT) {
      i -= NUM_DIRECT;
      if (i >= NUM_INDIRECT) {
        i = i % NUM_INDIRECT;
        buffer_cache_switch (data->doubly_indirect, (void **) &current);
        buffer_cache_switch (current[i / NUM_INDIRECT - 1], (void **) &current);
      }
      else
        buffer_cache_switch (data->indirect, (void **) &current);
    }

    retval = current[i];
    buffer_cache_release (current);
  }
  
  return retval;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = buffer_cache_get (sector);
  disk_inode->length = 0;
  disk_inode->magic = INODE_MAGIC;
  success = extend_inode_length (disk_inode, length);
  buffer_cache_release (disk_inode);
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
          buffer_cache_release (data);
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

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* Load sector into cache, then partially copy into caller's buffer. */
      void *cache_block = buffer_cache_get (sector_idx);
      memcpy (buffer + bytes_read, cache_block + sector_ofs, chunk_size);
      buffer_cache_release (cache_block);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* Load sector into cache, then partially copy into caller's buffer. */
      void *cache_block = buffer_cache_get (sector_idx);
      memcpy (cache_block + sector_ofs, buffer + bytes_written, chunk_size);
      buffer_cache_release (cache_block);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  return bytes_written;
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
  buffer_cache_release (data);
  return length;
}
