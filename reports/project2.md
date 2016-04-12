Student Testing Report
=========================================

### my-test-1
Tests syscalls for reading from standard input and writing to standard output. Input is read from stdin and then written to stdout. The output should be the same as the input.

The `write (STDOUT_FILENO, _, _)` syscall already receives coverage implicitly from other tests through the `msg` function provided in `lib.h`. However, the `read (STDIN_FILENO, _, _)` syscall receives zero coverage and could have been completely omitted from being implemented without failing any of the existing tests.

- If the kernel did not implement the read-stdin syscall, our test case would output nothing and fail the test.
- If the return values were too small, our test case would output a truncated message and fail the test.
- If the return values were too large, our test case would either output junk characters or cause a page fault.
- If the read-stdin syscall were otherwise implemented incorrectly, our test case would not print the correct message.

**Note**: line 70 in `tests/Make.tests` was modified to redirect text into the standard input buffer.

###### my-test-1.output
```
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
```
PASS

```

### my-test-2
This test is two-fold:
- Calls `exec` and `wait` on an instance of `create-bad-str`, which is another custom test case. This child process calls the `create` syscall with a pointer to a string that *begins* in user memory but *ends* somewhere in kernel memory. This should cause the child process to be killed by the kernel.
- When the parent resumes processing, it will attempt to `open` the text file `sample.txt`.

The second part of the test is to make sure that the syscall is not holding on to any locks when it fails a file system operation. This feature is not explicitly tested for in any of the other tests. It is tested *implicitly* in `multi-oom`, where some of the child processes kill themselves by calling `open` with an invalid pointer. However, seeing as how this line is placed in a random switch, we thought we should create a dedicated test for this feature.

- If the kernel only checks whether the string points to a user address, without verifying that the string also *terminates* at a user address, our test case will...
- If the syscall is still holding a lock on the file system when it terminates, our test will never finish.

###### my-test-2.output
```
Copying tests/userprog/my-test-2 to scratch partition...
Copying tests/userprog/create-bad-str to scratch partition...
qemu -hda /tmp/S7iMNqqIeu.dsk -m 4 -net none -nographic -monitor null
PiLo hda1
Loading..........
Kernel command line: -q -f extract run my-test-2
Pintos booting with 4,088 kB RAM...
382 pages available in kernel pool.
382 pages available in user pool.
Calibrating timer...  111,820,800 loops/s.
hda: 5,040 sectors (2 MB), model "QM00001", serial "QEMU HARDDISK"
hda1: 175 sectors (87 kB), Pintos OS kernel (20)
hda2: 4,096 sectors (2 MB), Pintos file system (21)
hda3: 200 sectors (100 kB), Pintos scratch (22)
filesys: using hda2
scratch: using hda3
Formatting file system...done.
Boot complete.
Extracting ustar archive from scratch device into file system...
Putting 'my-test-2' into the file system...
Putting 'create-bad-str' into the file system...
Erasing ustar archive...
Executing 'my-test-2':
(my-test-2) begin
(create-bad-str) begin
create-bad-str: exit(-1)
(my-test-2) end
my-test-2: exit(0)
Execution of 'my-test-2' complete.
Timer: 72 ticks
Thread: 30 idle ticks, 42 kernel ticks, 1 user ticks
hda2 (filesys): 139 reads, 406 writes
hda3 (scratch): 199 reads, 2 writes
Console: 985 characters output
Keyboard: 0 keys pressed
Exception: 0 page faults
Powering off...

```

###### my-test-2.result
```
PASS

```

Final Report for Project 2: User Programs
=========================================
