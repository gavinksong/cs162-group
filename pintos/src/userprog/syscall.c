#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);
int add_file_to_process(struct file *file_instance_);

/* Needed because only one process is allowed to access to modify the file. */
struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* args = ((uint32_t*) f->esp);
  printf("System call number: %d\n", args[0]);
  if (args[0] == SYS_EXIT) {
    f->eax = args[1];
    printf("%s: exit(%d)\n", &thread_current ()->name, args[1]);
    thread_current ()->pnode->exit_status = args[1];
    thread_exit();
  }
  else if (args[0] = SYS_PRACTICE) {
  	f->eax = args[1] + 1;
  }
  else if (args[0] = SYS_HALT) {
  	shutdown_power_off();
  }
  else if (args[0] = SYS_CREATE) {
  	lock_acquire(&file_lock);
    f->eax = filesys_create(args[1], args[2]);
    lock_release(&file_lock);
  }
  else if (args[0] = SYS_REMOVE) {
  	lock_acquire(&file_lock);
    f->eax = filesys_remove(args[1]);
    lock_release(&file_lock);
  }
  else if (args[0] = OPEN) {
    f->eax = open_file(args[1]);
  }
}

int open_file(const char *file) {
	lock_release(&file_lock);
  	struct file *file_instance_ = filesys_open(args[1]);
  	if (!file_instance_){
       return -1;
  	}
  	int fd = add_file_to_process(file_instance_);
  	lock_release(&file_lock);
  	return fd;
}

int add_file_to_process(struct file *file_instance_) {
	struct files *f = mallock(sizeof(struct files));
	f-> file_instance = file_instance_;
	f->fd = thread_current()->cur_fd++;
	list_push_back(&thread_current()->file_list, &f->elem);
	return f->fd;
}
