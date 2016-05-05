Final Report for Project 3: File System
=======================================

Student Testing Report
=========================================

### my-test-1
Test your buffer cache’s effectiveness by measuring its cache hit rate. First, reset the buffer cache.
Open a file and read it sequentially, to determine the cache hit rate for a cold cache. Then, close
it, re-open it, and read it sequentially again, to make sure that the cache hit rate improves.

We added `buffer_reset()` and `buffer_stat(int)` syscalls to reset the buffer_cache before any readings and get the number of cache hits/misses to calculate the hit rate. 

We remove the file created right before the test ends.

- If the kernel did not have buffer cache implemented, the writing procedure would be very long and the hit rate would not     improve at all
- If the kernel did not have the resetting buffer cache feature implement, the cache might not be cold in the first place and   the hit rate might not be accurate.

**Note**: line 7 in `tests/filesys/extended/Make.tests` was modified to run my-test-1

###### my-test-1.output
```
Copying tests/filesys/extended/my-test-1 to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu -hda /tmp/fdLMMRCEV1.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run my-test-1
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  104,755,200 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 186 sectors (93 kB), Pintos OS kernel (20)
hda2: 239 sectors (119 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-1' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'my-test-1':
(my-test-1) begin
(my-test-1) create "a"
(my-test-1) open "a"
(my-test-1) creating a
(my-test-1) close "tmp"
(my-test-1) resetting buffer
(my-test-1) open "a"
(my-test-1) read tmp
(my-test-1) close "a"
(my-test-1) open "a"
(my-test-1) read "a"
(my-test-1) close "a"
(my-test-1) Hit rate of the second reading is greater than hit rate of the first reading
(my-test-1) end
my-test-1: exit(0)
Execution of 'my-test-1' complete.
Timer: 98 ticks
Thread: 30 idle ticks, 50 kernel ticks, 19 user ticks
hdb1 (filesys): 337 reads, 563 writes
hda2 (scratch): 238 reads, 2 writes
Console: 1333 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

###### my-test-1.result
```
PASS
```

### my-test-2
This test is to check the buffer cache’s ability to write full blocks to disk without reading them first. 
200 blocks of contents are written into a file first and then use the syscall `buffer_stat(int)` to get the 
number of block_read. After writing 200 blocks to the file, the number of `block_read` is printed out, which is 1 
because there is a read and write of inode metadata.

We remove the file created right before the test ends.

- If the kernel makes unnecessary calls to block_read every time it writes a block to disk, the number of times block_read    is called would exceed 200.
- If the kernel does not support extensible files, then 200 blocks would be greater than the maximum file size. In this       case, number of block_writes called would be less than 200. Depending on how the kernel is implemented, there may also be   an error.

###### my-test-2.output
```
Copying tests/filesys/extended/my-test-2 to scratch partition...
Copying tests/filesys/extended/tar to scratch partition...
qemu -hda /tmp/Wuf1CGLTBJ.dsk -hdb tmp.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading...........
Kernel command line: -q -f extract run my-test-2
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  124,313,600 loops/s.
hda: 1,008 sectors (504 kB), model "QM00001", serial "QEMU HARDDISK"
hda1: 186 sectors (93 kB), Pintos OS kernel (20)
hda2: 236 sectors (118 kB), Pintos scratch (22)
hdb: 5,040 sectors (2 MB), model "QM00002", serial "QEMU HARDDISK"
hdb1: 4,096 sectors (2 MB), Pintos file system (21)
filesys: using hdb1
scratch: using hda2
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-2' into the file system...
Putting 'tar' into the file system...
Erasing ustar archive...
Executing 'my-test-2':
(my-test-2) begin
(my-test-2) create "a"
(my-test-2) open "a"
(my-test-2) write 100 kB to "a"
(my-test-2) called block_read 1 times in 200 writes
(my-test-2) called block_write 201 times in 200 writes
(my-test-2) close "a"
(my-test-2) end
my-test-2: exit(0)
Execution of 'my-test-2' complete.
Timer: 120 ticks
Thread: 32 idle ticks, 45 kernel ticks, 43 user ticks
hdb1 (filesys): 279 reads, 713 writes
hda2 (scratch): 235 reads, 2 writes
Console: 1147 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

###### my-test-2.result
```
PASS
```

### Our Experiences
Dispict the simplicity of the first test, we were confused about the meaning of the "sequential read" stated in the spec. 
Originally, we thought we were able to decide the reading order of the block of the file such that the cache hit rate would be improve in the second reading using clock algorithm. Besides that, we planed to use an article from Shakespeare, which is about 5MB, for the first test, but the article could not be loaded into the file system for testing. Thus, we switch to creating and writing a file inside the test function before doing the cache effectiveness test.



