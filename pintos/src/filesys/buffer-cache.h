#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include <stdbool.h>
#include "devices/block.h"

/* Stats. */
size_t cache_misses;    	/* Number of cache misses. */
size_t cache_hits;       	/* Number of cache hits. */

void buffer_cache_init (void);

/* Core interface. */
void *buffer_cache_get (block_sector_t sector);
void buffer_cache_release (void *cache_block, bool dirty);
void buffer_cache_flush (void);

/* For your convenience. */
void buffer_cache_read (block_sector_t sector, void *);
void buffer_cache_write (block_sector_t sector, void *);

/* Testing. */
void buffer_cache_reset (void);

#endif /* filesys/buffer-cache.h */
