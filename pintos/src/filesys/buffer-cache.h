#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include "devices/block.h"

void buffer_cache_init (void);

void *buffer_cache_get (block_sector_t sector);
void buffer_cache_release (void *cache_block);
void buffer_cache_switch (block_sector_t sector, void **cache_block);

void buffer_cache_read (block_sector_t sector, void *);
void buffer_cache_write (block_sector_t sector, void *);

#endif /* filesys/buffer-cache.h */