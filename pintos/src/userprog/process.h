#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/user/syscall.h"
#include "filesys/file.h"

struct pnode
  {
    pid_t pid;                /* Process identifier. */
    struct file *exe;         /* Executable file. */
    bool loaded;              /* True if executable has loaded successfully. */
    struct list_elem elem;    /* List element for children process list. */
    struct semaphore sema;    /* For signalling to parent. */
    int exit_status;          /* Default value of -1. */
  };

struct fnode
  {
    int fd;                         /* File descriptor. */
    const char *name;
    struct file *file;              /* The actual file instance. */
    struct list_elem elem;          /* List element for thread's file list */
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
