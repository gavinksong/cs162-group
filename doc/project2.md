Design Document for Project 2: User Programs
============================================

## Group Members

* Gavin Song <gavinsong93@berkeley.edu>
* Jun Tan <jtan0325@berkeley.edu>
* Pengcheng Liu <shawnpliu@berkeley.edu>


# Task 1: Argument Passing

### Data Structures and Functions

###### In process.c

```C
/* We only modify this function. */
static void start_process (void *file_name_);
```

### Algorithms

We use `strtok_r ()` to obtain pointers to the start of each token. This function also writes null characters between the tokens in the `file_name_`. To each pointer, we add the offset between the address of `file_name_` and its would-be address on the user stack and store the value in an array of `char` pointers `argv`. Meanwhile, we also count the number of arguments of store it in `argc`.

After the program is successfully loaded, we `memcpy ()` the region referenced by `file_name_` (including the newly added null pointers) onto the stack. Then, we jump to the next word boundary and `memcpy ()` the first `argc + 1` elements of `argv` onto the stack. Finally, we push a pointer to this copied array and the value of `argc` onto the stack. We point `if_.esp` to the top of the stack.

After this, the page containing `file_name_` is freed, and if the load had failed earlier, the thread exits. Otherwise, it jumps to the `intr_exit` stub, from which it returns and restores all of its register values from `if_`.

### Synchronization

I can't think of any synchronization issues.

### Rationale

It may be an unusual approach to copy over the entire command string to the stack, but this is obviously faster and saves us from having to write more code. Other than that, this was a straightforward task with little room for creativity. We just followed the directions.

# Task 2: Process Control Syscalls

### Data Structures and Functions

###### In process.h

```C
/* TODO: Add comments */
struct pnode {
  pid_t pid;
  bool loaded;
  struct list_elem elem;
  struct semaphore sema;
  int exit_status; //default is -1
}
```

###### In thread.h

```C
struct thread {
  ...
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    struct pnode* pnode;
    struct list children;
    ...
#endif
  ...
}
```

### Algorithms

All syscalls will be implemented in `syscall_handler ()`.

##### Practice

Take `args[1]`, increment it, and store it in the `eax` register of the interrupt frame.

##### Halt

Call `shutdown_power_off ()`.

##### Exec

Call `process_execute ()` on the input string, and obtain the tid of the launching process.

In `init_thread ()`, we will allocate a `pnode` struct for the new thread, add it to `children` of the current thread, and initialize `pnode->sema` to zero. When the thread finishes loading the program in `start_process ()`, it will increment `pnode->sema`.

Meanwhile, back in the `exec` system call, we will call `sema_down ()` on the pnode associated with the child whose `pid` equals the returned `tid`. Then, if the pnode has `loaded`, we will return the `pid` of the child. Otherwise, we will return -1.

##### Wait

Look for a pnode in the current thread's `children` list whose `pid` equals the given `pid`. If one does not exist, return -1. Otherwise, call `sema_down ()` on that pnode's semaphore. Then, return its `exit_status`, remove its `list_elem` from `children`, and free the pnode.

A thread will call `sema_up ()` on its pnode semaphore at the end of `process_exit ()`. The default value of `exit_status` will be -1, but if the `exit` syscall was used, this value will be overwritten with the provided value.

### Synchronization

### Rationale

Initially, we wanted to use a pair of locks instead of a semaphore to take advantage of priority donations, but we found it was too difficult to coordinate. The child process would need to acquire them before the parent, but there is no guarantee that the child will run before the parent returns from `thread_create`.

We decided to create a new struct to hold information about child processes that need to persist even after the child is terminated. Since it's possible for a child to load and terminate before the parent process returns from `thread_create ()`, this includes the semaphore used to signal the parent that the child has finished loading, as well as the boolean indicating success. We also found that pnodes were a better way for parent and child processes to keep track of each other, since a parent can easily maintain a linked list of pnodes, and it also made more sense for a child to possess a pointer to its pnode than directly to its parent (which was our original idea).

# Task 3: File Operation Syscalls

### Data Structures and Functions

###### In syscall.c

```C
/* Needed because only one process is allowed to access to modify the file. */
struct lock file_lock; 
'''

###### In syscall.h

'''C
struct files{
	int fd;
	struct file *file_instance;
	const char *file_name;
}
'''

###### In thread.h

'''C
/* Keep track of the files in belongs to the current process. */
struct list file_list; 
/* Keep track of the current max fd in the current process. Starts from 2. */
int cur_fd; 
'''

###### In syscall.c

'''C
/* Look for the file instane with the corresponding fd. */
struct files* get_file_instance_from_fd(int fd); 
/* Look for the file instane with the corresponding fileName. */
struct files* get_file_instance_from_name(int file_name_); 
/* Get the cur_Fd and increment by one because we only call this function when we open a file and need to assign a fd to the newly opened file and thus the increment it by one. */
int get_cur_fd(); 
'''

### Algorithms

Initialize the file_lock inside syscall_init.

Create: Acquire the file_lock to ensure that no process can intrupt the creation. Then use filesys_create to create the file. Release the file_lock after creation.

Remove: Get the FILES object and remove it from the current process's file_list. Then use filesys_remove to remove the file_instance of the FILES object.

Open: Use filesys_open to open the file and its return value is the file_instance. Get the next fd which is get_cur_fd() + 1. Create a FILES instance and put it into the current process's file_list.

Filesize: Get the FILES oject and call file_length on its file_instance.

Read: Get the FILES object and call file_read on its file_instance.

Write: Check the fd. If fd is 1, call lib/kernel/console.c putbuf(buffer, size). Otherwise, get the FILES object with the corresponding fd and call file_write on its file_instance.

Seek: Get the FILES object with the corresponding fd and call file_seek on its file_instance.

Tell: Get the FILES object with the corresponding fd and call file_tell on its file_instance.

Close: Get the FILES object with the corresponding fd and call file_close on its file_instance.

### Synchronization

In order to prevent intruption during any of the file syscalls, all of the above filesystem syscalls have to acquire the file_lock in the very beginning of the function and release the file_lock right before it returns. Hence, there should not have any intruption in the middle of the file modification. There should not be any synchronization issue.

### Rationale

There is another approach to store the files in an array, but the array has a fix size and we cannot modify the array size or remove the element after we initialize it, meanwhile we do not know the maximum amount of files a process can hold, we choose to use a linked-list structure instead. Obviousely, we access time of array is faster than the linked-list's, since it cannot be modify and the linked-list structure is provided already, we choose to maintain the files in the linked-list.
