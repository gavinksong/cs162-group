Design Document for Project 3: File System
==========================================

## Group Members

* Gavin Song <gavinsong93@berkeley.edu>
* Jun Tan <jtan0325@berkeley.edu>
* Pengcheng Liu <shawnpliu@berkeley.edu>


# Task 1: Buffer Cache

### Data Structures and Functions

###### In inode.c

```C
struct entry {
  uint32_t key;       /* Sector index. */
  uint8_t val;        /* Block index. */
  uint8_t next;       /* Entry index. */
}

/* Should occupy exactly 512 bytes of memory */
struct cache_metadata {
  entry[63] entries;
  char[122] buckets;
  struct bitmap refbits;
}
```
