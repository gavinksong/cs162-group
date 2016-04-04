#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"

struct fnode
  {
    int fd;                         /* File descriptor. */
    const char *name;
    struct file *file;     /* The actual file instance. */
    struct list_elem elem;          /* List element for thread's file list */
  };

static void syscall_handler (struct intr_frame *);
int add_file_to_process (struct file *file_);
struct fnode *get_file_from_fd (int fd);
void check_ptr (void *ptr, size_t size);
bool valid_addr (void *uaddr);

/* Needed because only one process is allowed to access to modify the file. */
struct lock file_lock;

void
syscall_init (void) 
{
  lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t* args = ((uint32_t*) f->esp);

  // Validate stack argument pointers
  check_ptr (args, sizeof (uint32_t));
  switch (args[0]) {
    case SYS_READ:
    case SYS_WRITE:
      check_ptr (&args[3], sizeof (uint32_t));
    case SYS_CREATE:
    case SYS_SEEK:
      check_ptr (&args[2], sizeof (uint32_t));
    case SYS_PRACTICE:
    case SYS_EXIT:
    case SYS_EXEC:
    case SYS_WAIT:
    case SYS_REMOVE:
    case SYS_OPEN:
    case SYS_FILESIZE:
    case SYS_TELL:
    case SYS_CLOSE:
      check_ptr (&args[1], sizeof (uint32_t));
  }

  switch (args[0]) {
    // For the following syscalls, args[1] is expected to be a user string.
    case SYS_EXEC:
    case SYS_CREATE:
    case SYS_REMOVE:
      check_ptr ((char *) args[1], strlen ((char *) args[1]) + 1);
      break;
    // For the following syscalls, args[2] is expected to be a buffer of size args[3].
    case SYS_WRITE:
    case SYS_READ:
      check_ptr ((void *) args[2], args[3]);
      break;
  }

  if (args[0] == SYS_EXIT) {
    thread_current ()->pnode->exit_status = args[1];
    thread_exit ();
  }
  else if (args[0] == SYS_PRACTICE)
    f->eax = args[1] + 1;
  else if (args[0] == SYS_HALT)
    shutdown_power_off ();
  else if (args[0] == SYS_EXEC)
    f->eax = process_execute ((char *) args[1]);
  else if (args[0] == SYS_WAIT)
    f->eax = process_wait (args[1]);
  else {
    // The remaining syscalls are all file system operations.
    lock_acquire (&file_lock);

    if (args[0] == SYS_CREATE)
      f->eax = filesys_create ((char *) args[1], args[2]);
    else if (args[0] == SYS_REMOVE)
      f->eax = filesys_remove ((char *) args[1]);
    else if (args[0] == SYS_OPEN) {
      struct file *file_ = filesys_open ((char *) args[1]);
      f->eax = file_ ? add_file_to_process (file_) : -1;
    }
    // Syscall for reading from stdin.
    else if (args[0] == SYS_READ && args[1] == 0) {
      uint8_t *buffer = (uint8_t *) args[2];
      size_t i = 0;
      while (i < args[3]) {
        buffer[i++] = input_getc ();
        if (buffer[i] == '\n')
          break;
      }
      f->eax = i;
    }
    // Syscall for writing to stdout.
    else if (args[0] == SYS_WRITE && args[1] == 1) {
      putbuf ((void *) args[2], args[3]);
      f->eax = args[3];
    }
    else {
      // For the remaining syscalls, args[1] is a file descriptor.
      struct fnode *fn = get_file_from_fd (args[1]);
      if (fn == NULL)
        f->eax = -1;
      if (args[0] == SYS_FILESIZE)
        f->eax = file_length (fn->file);
      else if (args[0] == SYS_READ)
        f->eax = file_read (fn->file, (void *) args[2], args[3]);
      else if(args[0] == SYS_WRITE)
        f->eax = file_write (fn->file, (void *) args[2], args[3]);
      else if(args[0] == SYS_SEEK)
        file_seek (fn->file, args[2]);
      else if(args[0] == SYS_TELL)
        f->eax = file_tell (fn->file);
      else if(args[0] == SYS_CLOSE) {
        file_close (fn->file);
        list_remove (&fn->elem);
        free (fn);
      }
    }
    lock_release (&file_lock);
  }
}

struct fnode *get_file_from_fd (int fd) {
  struct list *list_ = &thread_current ()->file_list;
  struct list_elem *e = list_begin (list_);
  for (; e != list_end (list_); e = list_next (e)) {
    struct fnode *f = list_entry (e, struct fnode, elem);
    if (f->fd == fd)
      return f;
  }
  return NULL;
}

int add_file_to_process(struct file *file_) {
  struct thread *t = thread_current ();
  struct fnode *f = malloc (sizeof (struct fnode));
  f->file = file_;
  f->fd = t->cur_fd++;
  list_push_back (&t->file_list, &f->elem);
  return f->fd;
}

bool valid_addr (void *uaddr) {
  return is_user_vaddr (uaddr) &&
         pagedir_get_page (thread_current ()->pagedir, uaddr) != NULL;
}

void check_ptr (void *ptr, size_t size) {
  if (!valid_addr (ptr) || !valid_addr (ptr + size))
    thread_exit ();
}
