#include "filesys/buffer-cache.h"
#include <bitmap.h>
#include <hash.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"

#define NUM_SECTORS 64

static void *cache_base;                        /* Points to the base of the buffer cache. */
static size_t clock_hand;                       /* Used for clock replacement. */
static struct entry *entries[NUM_SECTORS];      /* Array of cache entry refs. */
static struct bitmap *refbits;                  /* Reference bits for clock replacement. */
static struct bitmap *usebits;                  /* Marked for each locked entry. */
static struct hash hashmap;                     /* Maps sector indices to cache entries. */
static struct lock cache_lock;                  /* Acquire before accessing cache metadata. */
static struct condition cache_queue;            /* Block if all cache entries are in use. */

static void *index_to_block (size_t index);
unsigned hash_function (const struct hash_elem *e, void *aux);
bool less_function (const struct hash_elem *a, const struct hash_elem *b, void *aux);

struct entry
  {
    block_sector_t sector;
    size_t index;
    struct lock lock;
    struct hash_elem elem;
  };

/* Initializes the buffer cache. */
void
buffer_cache_init (void) 
{
  cache_base = palloc_get_multiple (PAL_ASSERT, 8);
  clock_hand = 0;
  refbits = bitmap_create (NUM_SECTORS);
  usebits = bitmap_create (NUM_SECTORS);
  hash_init (&hashmap, hash_function, less_function, NULL);
  lock_init (&cache_lock);
  cond_init (&cache_queue);
}

/* Checks if SECTOR is in the buffer cache, and if it is not,
   loads SECTOR into a cache block. Locks the corresponding
   cache entry until buffer_cache_release () is called.
   Returns the cache block containing the data from SECTOR. */
void *
buffer_cache_get (block_sector_t sector)
{
  struct entry *e = malloc (sizeof (struct entry));
  e->sector = sector;

  lock_acquire (&cache_lock);
  while (bitmap_all (usebits, 0, NUM_SECTORS)) {
    cond_wait (&cache_queue, &cache_lock);
  }

  struct hash_elem *found = hash_insert (&hashmap, &e->elem);
  if (found == NULL) {
    lock_init (&e->lock);

    while (bitmap_test (refbits, clock_hand) || bitmap_test (usebits, clock_hand)) {
      bitmap_reset (refbits, clock_hand);
      clock_hand = (clock_hand + 1) % NUM_SECTORS;
    }

    struct entry *old_entry = entries[clock_hand];
    if (old_entry != NULL) {
      block_write (fs_device, old_entry->sector, index_to_block (old_entry->index));
      free (old_entry);
    }

    e->index = clock_hand;
    entries[e->index] = e;
    clock_hand = (clock_hand + 1) % NUM_SECTORS;
  }
  else {
    free (e);
    e = hash_entry (found, struct entry, elem);
  }

  lock_acquire (&e->lock);
  bitmap_mark (usebits, e->index);
  block_read (fs_device, e->sector, index_to_block (e->index));

  lock_release (&cache_lock);

  return index_to_block (e->index);
}

/* Releases the lock on the cache entry associated with
   the block at CACHE_BLOCK. */
void
buffer_cache_release (void *cache_block)
{
  int index = (cache_block - cache_base) / BLOCK_SECTOR_SIZE;

  ASSERT (index >= 0 && index < NUM_SECTORS);
  ASSERT (lock_held_by_current_thread (&entries[index]->lock));

  lock_acquire (&cache_lock);

  lock_release (&entries[index]->lock);
  bitmap_mark (refbits, index);
  bitmap_reset (usebits, index);
  cond_signal (&cache_queue, &cache_lock);

  lock_release (&cache_lock);
}

/* Releases the entry associated with the block at *CACHE_BLOCK.
   Then, stores pointer to cache block holding data from SECTOR in
   *CACHE_BLOCK. */
void
buffer_cache_switch (block_sector_t sector, void **cache_block)
{
  buffer_cache_release (*cache_block);
  *cache_block = buffer_cache_get (sector);
}

/* Reads SECTOR into BUFFER. */
void
buffer_cache_read (block_sector_t sector, void *buffer)
{
  void *cache_block = buffer_cache_get (inode->sector);
  memcpy (buffer, cache_block, BLOCK_SECTOR_SIZE);
  buffer_cache_release (cache_block);
}

/* Writes BLOCK_SECTOR_SIZE bytes from BUFFER into the
   cache block holding SECTOR. */
void
buffer_cache_write (block_sector_t sector, void *buffer)
{
  void *cache_block = buffer_cache_get (inode->sector);
  memcpy (cache_block, buffer, BLOCK_SECTOR_SIZE);
  buffer_cache_release (cache_block);
}

/* Returns a pointer to the (INDEX + 1)th cache block. */
static void *
index_to_block (size_t index) {
  return cache_base + BLOCK_SECTOR_SIZE * index;
}

unsigned
hash_function (const struct hash_elem *e, void *aux UNUSED)
{
  return hash_entry (e, struct entry, elem)->sector;
}

bool
less_function (const struct hash_elem *a,
               const struct hash_elem *b,
               void *aux UNUSED)
{
  return hash_function (a, NULL) < hash_function (b, NULL);
}
