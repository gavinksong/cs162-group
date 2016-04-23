Design Document for Project 3: File System
==========================================

## Group Members

* Gavin Song <gavinsong93@berkeley.edu>
* Jun Tan <jtan0325@berkeley.edu>
* Pengcheng Liu <shawnpliu@berkeley.edu>


# Task 1: Buffer Cache

### Data Structures and Functions

```C
/* Buffer Cache */
void *cache_base;                 /* Points to the base of the buffer cache. */
size_t clock_hand;                /* Used for clock replacement. */
struct bitmap *refbits;           /* Reference bits for clock replacement. */
struct keyval *hash_table[64];    /* Maps sector indices to cache entries. */
struct lock cache_lock;           /* Acquire before accessing cache metadata. */
struct cond cache_queue;          /* Block when all cache entries are in use. */
uint32_t use_cnt[64];             /* Number of processes that are "using" each entry. */

struct keyval {
  uint32_t key,
  uint32_t val,
  struct keyval *next,
}

void *get_cache_block (block_sector_t sector_idx);
void release_cache_block (block_sector_t sector_idx);
```

### Algorithms

We will allocate 8 contiguous pages of memory to the buffer cache, using `palloc_get_multiple ()`. The pointer `cache_base` will be set to the base of this region, such that the start of the *(i+1)*th cache entry is located at `cache_base + BLOCK_SECTOR_SIZE * i`.

We will use a hashmap to map sectors to entries in the buffer cache. We will use the six least significant bits in the sector index for the hash.

`get_cache_block ()` starts by acquiring the `cache_lock` and then looking up the sector in the hash table. If the sector cannot be found in the hash table, we will use the clock algorithm to select a block for eviction. Our clock algorithm will ignore blocks that are currently "in use" (which are tracked by `use_cnt`). If there are currently no blocks that are not "in use", we will wait in the `cache_queue` instead of running the clock algorithm. When the thread unblocks, it repeats from the step following acquiring `cache_lock`.

### Synchronization

We put a lock on the buffer cache, so that the full set of actions required to "get" or "release" a cache block has to finish before moving on to the next set of actions. This is necessary because there are many gaps between when the metadata is read and when it is updated. For example, when a block is found in the hash table, we expect it to not be evicted before we update the use bit and the ref bit. Conversely, when a block is not found in the hash table, we expect it not to be added to the cache before we add it to the cache ourselves, and we expect the metadata to reflect that.

In `get_cache_block ()`, the "use count" corresponding to the returned block is incremented. We have a separate function `release_cache_block ()` for decrementing this value, as well as signalling `cache_queue` if this causes the count to hit zero. The purpose of the use count is to ensure that the block is not evicted from the cache before the kernel is done with it, while still allowing other threads to simultaneously access the cache block.

### Rationale

We use a simple hashmap to improve lookup speeds. We use the six least significant bytes of the sector index to avoid collisions due to spatial locality. It's also slightly faster using a masking operation. We have 64 entries in the hash table because it is a power of two and gives us a load factor of 1... which is probably good enough.

Every entry in the hash table is a linked list of key-value pairs. However, we don't use the linked list implementation provided by Pintos - for three reasons. One, we don't expect these lists to be very long. In fact, we expect them to contain one entry on average. Yet, every Pintos linked list has a head and a tail node, and that would make each list several times larger than necessary. Two, a `list_elem` needs to be embedded inside of another struct in order to be associated with actual data. Since we would have to create a separate struct anyways, it makes little sense to include a `list_elem` when we could accomplish the same thing with a pointer to the next key-value pair. And finally, three, we only need to be able to traverse the list forwards, one element at a time, so we have no need for the additional functionality provided by `list.h`.

# Task 2: Entensible Files
```C
struct inode_disk {
  block_sector_t parent_dir;         /* Task 3. */
  block_sector_t direct[122];        
  block_sector_t indirect;
  block_sector_t doubly_indirect;
  off_t length;                      
  bool is_dir;
  unsigned magic; // magic is in a different place, 
  uint8_t unused[3];
};
```

### Algorithms
Whenever a file or a directory is created, free_map_allocate will allocate blocks for a given length. If the last block allocated is not fully occupied, still allocate that block. 
Inside the inode_write_at function, if the size of the data to be written cannot fit in the blocks have been allocated, then use the free_map_allocate to allocate blocks for the excessive data. Because free_map_allocate allocates a chunk of consecutive blocks, it may happens to be that all the consecutive blocks are too small to hold all the excessive data and free_map_allocate fails. In this case, use free_map_allocate to allocate one block at a time. 
Remove the "data" member from the inode struct and use the "sector" member to keep track of the inode_disk instead. Thus byte_to_sector and inode_length function should be modified to adapt to this change.

### Synchronization
In this section, there should not be any synchronization issue because task 1 takes care all of that

### Rationale

# Task 3: Subdirectories

### Data Structures and Functions

```C
struct inode_disk {
  block_sector_t parent_dir;      /* The sector . */
  bool is_dir;                      
  ...
};
```

### Algorithms
