#include "filesys/buffer-cache.h"
#include <bitmap.h>
#include <hash.h>
#include <string.h>
#include "filesys/filesys.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "devices/timer.h"

#define NUM_SECTORS 64
#define WRITE_DELAY 30000

static void *cache_base;                        /* Points to the base of the buffer cache. */
static size_t clock_hand;                       /* Used for clock replacement. */
static struct entry *entries[NUM_SECTORS];      /* Array of cache entry refs. */
static struct bitmap *refbits;                  /* Reference bits for clock replacement. */
static struct bitmap *usebits;                  /* Marked for each locked entry. */
static struct hash hashmap;                     /* Maps sector indices to cache entries. */
static struct lock cache_lock;                  /* Acquire before accessing cache metadata. */
static struct condition cache_queue;            /* Block if all cache entries are in use. */

static void *index_to_block (size_t index);
static bool find_entry (block_sector_t sector, struct entry **);
unsigned hash_function (const struct hash_elem *e, void *aux);
bool less_function (const struct hash_elem *a, const struct hash_elem *b, void *aux);
void write_behind_thread_func (void *aux);

struct entry
  {
    block_sector_t sector;
    size_t index;
    struct condition queue;
    struct hash_elem elem;
    bool dirty;
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
  thread_create ("write-behind", PRI_MAX, write_behind_thread_func, NULL);

  // Stats.
  cache_misses = 0;
  cache_hits = 0;
}

/* Checks if SECTOR is in the buffer cache, and if it is not,
   loads SECTOR into a cache block. "Locks" the corresponding
   cache entry until buffer_cache_release () is called.
   Returns the cache block containing SECTOR's contents. */
void *
buffer_cache_get (block_sector_t sector)
{
  struct entry *e;
  bool cache_hit;

  lock_acquire (&cache_lock);
  cache_hit = find_entry (sector, &e);
  lock_release (&cache_lock);

  if (!cache_hit)
    block_read (fs_device, sector, index_to_block (e->index));

  return index_to_block (e->index);
}

/* Releases the "lock" on the cache entry associated with
   the block at CACHE_BLOCK. The parameter DIRTY should
   be marked true if the contents of CACHE_BLOCK were
   modified since it was returned by buffer_cache_get (). */
void
buffer_cache_release (void *cache_block, bool dirty)
{
  int index = (cache_block - cache_base) / BLOCK_SECTOR_SIZE;

  ASSERT (index >= 0 && index < NUM_SECTORS);
  ASSERT (bitmap_test (usebits, index));

  lock_acquire (&cache_lock);

  if (dirty)
    entries[index]->dirty = true;

  bitmap_mark (refbits, index);
  bitmap_reset (usebits, index);
  cond_signal (&entries[index]->queue, &cache_lock);
  cond_signal (&cache_queue, &cache_lock);

  lock_release (&cache_lock);
}

/* Flushes all dirty cache entries to disk. */
void
buffer_cache_flush (void)
{
  size_t i = 0;
  lock_acquire (&cache_lock);
  for (; i < NUM_SECTORS; i++)
    if (entries[i] != NULL && entries[i]->dirty && !bitmap_test (usebits, i)) {
      block_write (fs_device, entries[i]->sector, index_to_block (i));
      entries[i]->dirty = false;
    }
  lock_release (&cache_lock);
}

/* Reads SECTOR into BUFFER. */
void
buffer_cache_read (block_sector_t sector, void *buffer)
{
  void *cache_block = buffer_cache_get (sector);
  memcpy (buffer, cache_block, BLOCK_SECTOR_SIZE);
  buffer_cache_release (cache_block, false);
}

/* Writes BLOCK_SECTOR_SIZE bytes from BUFFER into SECTOR. */
void
buffer_cache_write (block_sector_t sector, void *buffer)
{
  struct entry *e;

  lock_acquire (&cache_lock);
  find_entry (sector, &e);
  lock_release (&cache_lock);

  void *cache_block = index_to_block (e->index);
  memcpy (cache_block, buffer, BLOCK_SECTOR_SIZE);
  buffer_cache_release (cache_block, true);
}

/* Resets the cache and stats.
   May PANIC if cache is in use.
   Use only for testing purposes. */
void
buffer_cache_reset (void)
{
  buffer_cache_flush ();

  lock_acquire (&cache_lock);

  /* It's your fault if this causes a panic. */
  ASSERT (bitmap_none (usebits, 0, NUM_SECTORS));

  /* Clear all entries. */
  size_t i;
  for (i = 0; i < NUM_SECTORS; i++)
    if (entries[i] != NULL) {
      hash_delete (&hashmap, &entries[i]->elem);
      free (entries[i]);
      entries[i] = NULL;
    }

  ASSERT (hash_size (&hashmap) == 0);

  bitmap_set_all (refbits, false);

  /* Reset stats. */
  cache_misses = 0;
  cache_hits = 0;

  lock_release (&cache_lock);
}

/* Returns a pointer to the (INDEX + 1)th cache block. */
static void *
index_to_block (size_t index) {
  return cache_base + BLOCK_SECTOR_SIZE * index;
}

/* Checks if SECTOR is in the buffer cache, and if it is not,
   allocates a cache block for it. "Locks" the corresponding
   cache entry and stores it in ENTRY.
   Returns true on a cache hit, false otherwise. */
static bool
find_entry (block_sector_t sector, struct entry **entry)
{
  struct entry *e = malloc (sizeof (struct entry));
  e->sector = sector;

  /* Wait if all the cache blocks are in use. */
  while (bitmap_all (usebits, 0, NUM_SECTORS))
    cond_wait (&cache_queue, &cache_lock);

  struct hash_elem *found = hash_insert (&hashmap, &e->elem);
  if (found == NULL) {
    cache_misses++;

    /* Clock algorithm. */
    while (bitmap_test (refbits, clock_hand) || bitmap_test (usebits, clock_hand)) {
      bitmap_reset (refbits, clock_hand);
      clock_hand = (clock_hand + 1) % NUM_SECTORS;
    }

    /* Evict entry and write contents to disk. */
    struct entry *old_entry = entries[clock_hand];
    if (old_entry != NULL) {
      block_write (fs_device, old_entry->sector, index_to_block (old_entry->index));
      hash_delete (&hashmap, &old_entry->elem);
      free (old_entry);
    }

    /* Initialize new entry. */
    cond_init (&e->queue);
    e->dirty = false;
    e->index = clock_hand;
    entries[e->index] = e;
    clock_hand = (clock_hand + 1) % NUM_SECTORS;
  }
  else {
    cache_hits++;
    free (e);
    e = hash_entry (found, struct entry, elem);
  }

  /* Wait for your turn to acquire entry. */
  while (bitmap_test (usebits, e->index))
    cond_wait (&e->queue, &cache_lock);
  bitmap_mark (usebits, e->index);

  *entry = e;
  return (found != NULL);
}

/* Just returns the sector number. The hash map automagically
   grows its number of buckets in powers of two and masks
   off the appropriate number of higher nibble bits. */
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

/* High-priority write-behind thread. */
void
write_behind_thread_func (void *aux UNUSED) {
  while (true) {
    timer_msleep (WRITE_DELAY);
    buffer_cache_flush ();
  }
}
