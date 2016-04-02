#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/user/syscall.h"

struct pnode
  {
	pid_t pid;					/* Process identifier. */
	bool loaded;				/* True if process has loaded successfully. */
	struct list_elem elem;		/* List element for children process list. */
	struct semaphore sema;		/* For signalling to parent. */
	int exit_status;			/* Default value of -1. */
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
