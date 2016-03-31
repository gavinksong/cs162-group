#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
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
    f->eax = filesys_create(args[1], args[2]);
  }
  else if (args[0] = SYS_REMOVE) {
    f->eax = filesys_remove(args[1]);
  }
  else if (args[0] = OPEN) {
    f->eax = filesys_open(args[1]);
  }
}
