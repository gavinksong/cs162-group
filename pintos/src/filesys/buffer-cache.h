#ifndef FILESYS_BUFFER_CACHE_H
#define FILESYS_BUFFER_CACHE_H

#include "devices/block.h"

void buffer_cache_init (void);
void *get_cache_block (block_sector_t sector);
void release_cache_block (void *cache_block);

#endif /* filesys/buffer-cache.h */