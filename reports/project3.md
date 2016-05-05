Final Report for Project 3: File System
=======================================

### my-test-1
Test your buffer cache’s effectiveness by measuring its cache hit rate. First, reset the buffer cache.
Open a file and read it sequentially, to determine the cache hit rate for a cold cache. Then, close
it, re-open it, and read it sequentially again, to make sure that the cache hit rate improves.

We added `buffer_reset()` and `buffer_stat(int)` syscalls to reset the buffer_cache before any readings and get the number of cache hits/misses to calculate the hit rate. 

**Note**: line 7 in `tests/filesys/extended/Make.tests` was modified to run my-test-1

###### my-test-1.output

###### my-test-1.result

### my-test-2
This test is to check the buffer cache’s ability to write full blocks to disk without reading them first. 
200 blocks of contents are written into a file first and then use the syscall `buffer_stat(int)` to get the 
number of block_read. After writing 200 blocks to the file, the number of `block_read` is printed out, which is 1 
because there is a read of inode metadata.

###### my-test-2.output

###### my-test-2.result

### Our Experiences
Dispict the simplicity of the first test, we were confused about the meaning of the "sequential read" stated in the spec. 
Originally, we thought we were able to decide the reading order of the block of the file such that the cache hit rate would be improve in the second reading using clock algorithm. Besides that, we planed to use an article from Shakespeare, which is about 5MB, for the first test, but the article could not be loaded into the file system for testing. Thus, we switch to creating and writing a file inside the test function before doing the cache effectiveness test.



