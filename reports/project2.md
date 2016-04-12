Final Report for Project 2: User Programs
=========================================

Student Testing Report
=========================================

### my-test-1
Tests syscalls for reading from standard input and writing to standard output. Input is read from stdin and then written to stdout. The output should be the same as the input.

The `write (STDOUT_FILENO, _, _)` syscall already receives coverage implicitly from other tests through the `msg` function provided in `lib.h`. However, the `read (STDIN_FILENO, _, _)` syscall receives zero coverage and could have been completely omitted from being implemented without failing any of the existing tests.

- If the kernel did not implement the read-stdin syscall, our test case would output nothing and fail the test.
- If the return values were too small, our test case would output a truncated message and fail the test.
- If the return values were too large, our test case would either output junk characters or cause a page fault.
- If the read-stdin syscall were otherwise implemented incorrectly, our test case would not print the correct message.

###### my-test-1.output
```C
Copying tests/userprog/my-test-1 to scratch partition...
qemu -hda /tmp/dvHu6kShRr.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading..........
Kernel command line: -q -f extract run my-test-1
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  104,755,200 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 175 sectors (87 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 101 sectors (50 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-1' into the file system...
Erasing ustar archive...
Executing 'my-test-1':
(my-test-1) begin
Hello, CS162!
(my-test-1) end
my-test-1: exit(0)
Execution of 'my-test-1' complete.
Timer: 56 ticks
Thread: 30 idle ticks, 26 kernel ticks, 0 user ticks
hda2 (filesys): 61 reads, 206 writes
hda3 (scratch): 100 reads, 2 writes
Console: 900 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...
```

###### my-test-1.result
```C
PASS
```
 
My_test_2: 

Description:

  Create a file with file_name goes beyond the user space. Then exec a child process trying to open (some operation) on a different file, say example.txt. 
  
  Before the file is created, the file_name is checked if it is mapped and within the user space. The create function should return -1 because the file_name is not valid. If the lock is not released properly when create function fails, the following child process is not able to do any operations on a different file and stays there forever waiting for the lock, which is not supposed to happen. 

Kernel bug:
  - if the file operations does not check if the passed in file_name is not checked if it is mapped and within the user space, the create function would not cause any error
  -  

