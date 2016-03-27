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
### Algorithms
### Synchronization
### Rationale

# Task 3: File Operation Syscalls

### Data Structures and Functions
### Algorithms
### Synchronization
### Rationale
