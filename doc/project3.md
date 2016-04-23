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

# Task 2: Extensible Files

###### inode.c

```C
struct inode_disk {
  ...
  block_sector_t direct[122];           /* Direct pointers. */
  block_sector_t indirect;              /* Indirect pointer. */
  block_sector_t doubly_indirect;       /* Doubly indirect pointer. */
  unsigned magic;                       /* Note: magic has a different offset now. */
  uint8_t unused[3];
};

struct inode {
  /* REMOVE struct inode_disk data. */
}
```

###### free-map.c

```C
static struct lock *free_map_lock;      /* Lock for free map operations. */

/* Allocates CNT sectors from the free map and stores each
   of them in sectors array. Returns true if successful,
   false if not enough consecutive sectors were available
   or if the free_map file could not be written. */
bool free_map_allocate_nc (size_t cnt, block_sector_t *sectors);
```

### Algorithms

The `data` member of the `inode` struct was removed. Instead, the data in `inode_disk` will always be accessed through the buffer cache. The access pattern will look something like this:

```C
struct inode_disk *data = get_cache_block (inode->sector);
/* Read or write to data here. */
release_cache_block (inode->sector);
```

Currently, `free_map_allocate ()` to allocate blocks for inodes, failing if it cannot find a single continuous region large enough to hold all of the blocks. This makes us susceptible to external fragmentation, so we would like to be able to allocate nonconsecutive blocks. We will add a function `free_map_allocate_nc ()` to accomplish this. First, it will count the number of free blocks available, failing fast if there are not enough. Then, it will allocate `cnt` blocks and return their sector indices.

When writing past the EOF of a file in `inode_write_at ()`, we need to allocate new blocks on the fly. We also need to modify `inode_create ()` to accomodate to the new inode structure. These will both use `free_map_allocate_nc ()`.

### Synchronization

We're putting a lock on the freemap. Using `free_map_allocate_nc ()` instead of repeatedly allocating single blocks of memory will ensure that either all blocks are allocated or none at all.

### Rationale

We packed `inode_disk` with direct pointers to maximize the benefit of faster accesses for short files. It was sort of an arbitrary decision though.

# Task 3: Subdirectories

### Data Structures and Functions

###### node.c

```C
struct inode_disk {
  block_sector_t parent_dir;          /* inode_disk sector of the parent directory. */
  bool is_dir;                        /* True if this file is a directory. */
  uint32_t num_files;                 /* The number of subdirectories or files. */
  ...
};

struct thread {
  ...
#ifdef USERPROG
  struct inode *cwd;                  /* The current working directory. */
  ...
};
```

###### inode.h

```C
/* Added a third parameter. */
bool inode_create (block_sector_t, off_t, bool is_dir);
```

###### filesys.h

```C
/* Added a fourth parameter. */
bool filesys_create (const char *name, off_t initial_size, bool is_dir);
```

### Algorithms

At startup, set every process's `cwd` to be `ROOT_DIR_SECTOR`. We are going to use the provided `get_next_part()` function to parse out the path.

`is_dir` and `parent_dir` are initialized when the directory is created. `filesys_create()` and `inode_create()` each take an additional parameter `is_dir`, which is true if the file being created is a directory. This is to correctly set the `is_dir` member of the `inode_disk` structure on creation.

For filesys operations, if we are given a relative path, we will begin traversing at the current process' `cwd` instead of the root directory. We repeatedly use `lookup` and `get_next_part ()` to follow the path. When we encounter a `..` in the path, then we traverse to the parent directory, whose location is stored in the `parent_dir` member of `inode_disk`. If a file can not be found or if one of the files in the middle of the path is not a directory, then the file operation fails.

`chdir()`: traverse the given path, then set the `cwd` of the current process to its destination.

`mkdir()`: If the directory trying to created already exists or the parent directory of the directory trying to be created does not already exist, return -1. Otherwise, use `filesys_create()` to create the directory with the parameter isDir = True. And increments the `num_files` of the parent directory

`readdir()`: get the object from the current process's file list. If the oject is a file or does not exist such a directory with the corresponding fd, return -1. Otherwise use the `dir_readdir()` to read the directory.

`isdir()`: use `process_get_file ()` to get the object. If the object is not a dir or NULL, return -1. Otherwise, use the object to get the inode, use that inode to get the `sector` which points to the struct `inode_disk` and checks the `is_dir` member in there.

`inumber()`:  use process_get_file to get the object. If the object is not a dir or NULL, return -1. Otherwise, use the object to get the inode and call `inode_get_inumber (const struct inode *inode)` with it.

`open()`: if the object being opened is a directory, add the object to the file_list and change the `file` member of the `fnode` to null and `dir` member to the directory object.

`close()`: if the fd is corresponding to a direcotry, do not do anything.

`remove()`: use process_get_file to get the object. If the object is not a dir or NULL, return -1. Otherwise, use the object to get the `dir` member, which contains a `inode`. Use the `inode` to get the `sector` of the inode_disk which tells us the 'num_files'. If the `num_files` is 0, call `dir_remove()`. Otherwise, removement should not be performed.

### Synchronization

We decided to disallow removing files or directories for which there is an `inode` with an `open_cnt` larger than 0. Also, we decided to treat each thread as an opener of its current working directory. This prevents current working directories from being deleted. Furthermore, we decided to disallow removing directories that are not empty.

### Rationale

We added a `num_files` member to the `inode_disk` structure in order to provide an easy way to determine whether a directory was empty and safe to remove.

# Additional Question
1. One way to implement write behind is to have the timer interrupt write dirty blocks to disk every 30 seconds. Each block in the cache would have an associated lock, which the timer interrupt would acquire before writing the block to disk, to prevent user programs from overwriting the data part way through. 

One way to implement read ahead is to use ide.c found in the devices folder. Create a list called read_ahead in the kernel that contains pointers to the blocks to be read in the future. Inode_read will populate this list whenever the length of text left to read is greater than the size of the current block it is reading. Then, in the interrupt_handler in ide.c, check the read_ahead list, and load the blocks into the cache buffer.  This way, future blocks can be read into the cache buffer asynchronously.
