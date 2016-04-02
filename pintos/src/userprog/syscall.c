#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "userprog/process.h"

/* All pointers for this part must be greater than this addr. */
#define BOTTOM_OF_USER_ADDR ((void *) 0x08048000)

struct fnode
  {
    int fd;                         /* File descriptor. */
    const char *name;
    struct file *file;     /* The actual file instance. */
    struct list_elem elem;          /* List element for thread's file list */
  };

static void syscall_handler (struct intr_frame *);
int add_file_to_process (struct file *file_);
struct pnode *get_child_pnode (pid_t pid);
struct fnode *get_file_from_fd (int fd);

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
  printf ("System call number: %d\n", args[0]);
  if (args[0] == SYS_EXIT) {
    f->eax = args[1];
    printf ("%s: exit(%d)\n", &thread_current ()->name, args[1]);
    thread_current ()->pnode->exit_status = args[1];
    thread_exit ();
  }
  else if (args[0] == SYS_PRACTICE) {
    f->eax = args[1] + 1;
  }
  else if (args[0] == SYS_HALT) {
    shutdown_power_off ();
  }
  else if (args[0] == SYS_EXEC) {
    pid_t pid = process_execute (args[1]);
    struct pnode *p = get_child_pnode (pid);
    sema_down (&p->sema);
    f->eax = p->loaded ? pid : -1;
  }
  else if (args[0] == SYS_WAIT) {
    struct pnode *p = get_child_pnode (args[1]);
    if (p != NULL) {
      sema_down (&p->sema);
      f->eax = p->exit_status;
      list_remove (&p->elem);
      free (p);
    }
    else
      f->eax = -1;
  }
  else {
    lock_acquire (&file_lock);
    if (args[0] == SYS_CREATE)
      f->eax = filesys_create (args[1], args[2]);
    else if (args[0] == SYS_REMOVE)
      f->eax = filesys_remove (args[1]);
    else if (args[0] == SYS_OPEN) {
      struct file *file_ = filesys_open (args[1]);
      f->eax = file_ ? add_file_to_process (file_) : -1;
    }
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
    else if (args[0] == SYS_WRITE && args[1] == 1) {
      putbuf (args[2], args[3]);
      f->eax = args[3];
    }
    else {
      struct fnode *fn = get_file_from_fd (args[1]);
      if (fn == NULL)
        f->eax = -1;
      if (args[0] == SYS_FILESIZE)
        f->eax = file_length (fn->file);
      else if (args[0] == SYS_READ)
        f->eax = file_read (fn->file, args[2], args[3]);
      else if(args[0] == SYS_WRITE)
        f->eax = file_write (fn->file, args[2], args[3]);
      else if(args[0] == SYS_SEEK)
        f->eax = file_seek (fn->file, args[2]);
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

struct pnode *get_child_pnode (pid_t pid)
{
  struct list list_ = thread_current ()->children;
  struct list_elem *e = list_begin (&list_);
  for (; e != list_end (&list_); e = list_next (e)) {
    struct pnode *p = list_entry (e, struct pnode, elem);
    if (p->pid == pid)
      return p;
  }
  return NULL;
}

struct fnode *get_file_from_fd (int fd) {
  struct list list_ = thread_current ()->file_list;
  struct list_elem *e = list_begin (&list_);
  for (; e != list_end (&list_); e = list_next (e)) {
    struct fnode *f = list_entry (e, struct fnode, elem);
    if (f->fd == fd)
      return f;
  }
  return NULL;
}

int add_file_to_process(struct file *file_) {
  struct fnode *f = malloc (sizeof (struct fnode));
  f->file = file_;
  f->fd = thread_current ()->cur_fd++;
  list_push_back (&thread_current ()->file_list, &f->elem);
  return f->fd;
}
