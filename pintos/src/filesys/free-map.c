#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/synch.h"

static struct file *free_map_file;   /* Free map file. */
static struct bitmap *free_map;      /* Free map, one bit per sector. */

/* We maintain a lock to synchronize free map operations that
   can also be acquired from the outside. If the lock is already
   held by the current thread, we ignore it. See macros below. */

static bool ignore_lock;

#define acquire_lock(LOCK) do {                                 \
  ignore_lock = lock_held_by_current_thread (LOCK);             \
  if (!ignore_lock)                                             \
    lock_acquire (LOCK);                                        \
} while (0)

#define release_lock(LOCK) do {                                 \
  if (!ignore_lock)                                             \
    lock_release (LOCK);                                        \
} while (0)

/* Initializes the free map. */
void
free_map_init (void)
{
  free_map = bitmap_create (block_size (fs_device));
  if (free_map == NULL)
    PANIC ("bitmap creation failed--file system device is too large");
  bitmap_mark (free_map, FREE_MAP_SECTOR);
  bitmap_mark (free_map, ROOT_DIR_SECTOR);
  lock_init (&free_map_lock);
}

/* Allocates CNT consecutive sectors from the free map and stores
   the first into *SECTORP.
   Returns true if successful, false if not enough consecutive
   sectors were available or if the free_map file could not be
   written. */
bool
free_map_allocate (size_t cnt, block_sector_t *sectorp)
{
  acquire_lock (&free_map_lock);
  block_sector_t sector = bitmap_scan_and_flip (free_map, 0, cnt, false);

  if (sector != BITMAP_ERROR
      && free_map_file != NULL
      && !bitmap_write (free_map, free_map_file))
    {
      bitmap_set_multiple (free_map, sector, cnt, false);
      sector = BITMAP_ERROR;
    }
  release_lock (&free_map_lock);
  if (sector != BITMAP_ERROR)
    *sectorp = sector;
  return sector != BITMAP_ERROR;
}

/* Makes CNT sectors starting at SECTOR available for use. */
void
free_map_release (block_sector_t sector, size_t cnt)
{
  ASSERT (bitmap_all (free_map, sector, cnt));
  acquire_lock (&free_map_lock);
  bitmap_set_multiple (free_map, sector, cnt, false);
  bitmap_write (free_map, free_map_file);
  release_lock (&free_map_lock);
}

/* Allocates CNT nonconsecutive sectors from the free map and stores
   each of them in *SECTORS.
   Returns true if successful, false if not enough sectors were
   available. */
bool
free_map_allocate_nc (size_t cnt, block_sector_t *sectors)
{
  bool success = false;
  acquire_lock (&free_map_lock);
  if (bitmap_count (free_map, 0, block_size (fs_device), false) >= cnt) {
    size_t i = 0;
    size_t pos = 0;
    for (; i < cnt; i++) {
      pos = bitmap_scan_and_flip (free_map, pos, 1, false);
      sectors[i] = pos++;
    }
    if (free_map_file != NULL)
      bitmap_write (free_map, free_map_file);
    success = true;
  }
  release_lock (&free_map_lock);
  return success;
}

/* Makes the first CNT sectors in SECTORS available for use. */
void
free_map_release_nc (block_sector_t *sectors, size_t cnt)
{
  acquire_lock (&free_map_lock);
  size_t i = 0;
  for (; i < cnt; i++) {
    ASSERT (bitmap_test (free_map, sectors[i]));
    bitmap_reset (free_map, sectors[i]);
    sectors[i] = 0;
  }
  bitmap_write (free_map, free_map_file);
  release_lock (&free_map_lock);
}

/* Opens the free map file and reads it from disk. */
void
free_map_open (void)
{
  acquire_lock (&free_map_lock);
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_read (free_map, free_map_file))
    PANIC ("can't read free map");
  release_lock (&free_map_lock);
}

/* Writes the free map to disk and closes the free map file. */
void
free_map_close (void)
{
  acquire_lock (&free_map_lock);
  file_close (free_map_file);
  release_lock (&free_map_lock);
}

/* Creates a new free map file on disk and writes the free map to
   it. */
void
free_map_create (void)
{
  /* Create inode. */
  if (!inode_create (FREE_MAP_SECTOR, bitmap_file_size (free_map), false))
    PANIC ("free map creation failed");

  acquire_lock (&free_map_lock);

  /* Write bitmap to file. */
  free_map_file = file_open (inode_open (FREE_MAP_SECTOR));
  if (free_map_file == NULL)
    PANIC ("can't open free map");
  if (!bitmap_write (free_map, free_map_file))
    PANIC ("can't write free map");

  release_lock (&free_map_lock);
}

/* Returns the number of sectors available for use. */
size_t
free_map_available_space (void)
{
  acquire_lock (&free_map_lock);
  size_t space = bitmap_count (free_map, 0, block_size (fs_device), false);
  release_lock (&free_map_lock);
  return space;
}
