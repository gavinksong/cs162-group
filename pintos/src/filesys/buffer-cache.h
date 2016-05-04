#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include <stdbool.h>
#include "devices/block.h"

/* Counters for Testing */
size_t cache_total;                      /* Total requests to cache */
size_t cache_hit;                        /* Cache hits */
size_t nblock_read;                      /* Number of block_read */
size_t nblock_write;                     /* Number of block_write */

void buffer_cache_init (void);

/* Core interface. */
void *buffer_cache_get (block_sector_t sector);
void buffer_cache_release (void *cache_block, bool dirty);
void buffer_cache_flush (void);

/* For your convenience. */
void buffer_cache_read (block_sector_t sector, void *);
void buffer_cache_write (block_sector_t sector, void *);

#endif /* filesys/buffer-cache.h */
