#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct files {
	/* Keep track of the file descriptor. */
    int fd;
    /* The actual file instance. */
    struct file *file_instance;
    /* The file created with the giveName. */
    const char *file_name;
    /* Needed for the list_push_back(...) function. */
    struct list_elem elem;
};

void syscall_init (void);

#endif /* userprog/syscall.h */
