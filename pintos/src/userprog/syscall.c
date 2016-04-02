#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "userprog/process.h"

struct fnode
  {
    int fd;                         /* File descriptor. */
    const char *name;
    struct file *file;     /* The actual file instance. */
    struct list_elem elem;          /* List element for thread's file list */
  };

static void syscall_handler (struct intr_frame *);
int add_file_to_process (struct file *file_);

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
  else if (args[0] == SYS_CREATE) {
    lock_acquire (&file_lock);
    f->eax = filesys_create (args[1], args[2]);
    lock_release (&file_lock);
  }
  else if (args[0] == SYS_REMOVE) {
    lock_acquire (&file_lock);
    f->eax = filesys_remove (args[1]);
    lock_release (&file_lock);
  }
  else if (args[0] == OPEN) {
    lock_acquire (&file_lock);
    struct file *file_ = filesys_open (args[1]);
    if (!file_)
        f->eax = -1;
    else
      f->eax = add_file_to_process (file_);
    lock_release (&file_lock);
  }
  else if (args[0] == SYS_FILESIZE) {
    lock_acquire (&file_lock);
    struct fnode *f = get_file_from_fd (args[1]); 
    if (f == NULL)
      f->eax = -1;
    else
      f->eax = file_length (f->file);
    lock_release (&file_lock);
  }
  else if (args[0] == SYS_READ) {
    lock_acquire (&file_lock);
    int fd = args[1];
    void *buffer = (void *) args[2];
    unsigned size = args[3];
    if (fd == 0) {
      unsigned start;
      for (start = 0; start < size; start ++) {
        buffer[start] = input_getc ();
      }
      f->eax = size;
    }
    else {
      struct fnode *f = get_file_from_fd (fd); 
      if (f == NULL)
        f->eax = -1;
      else
        f->eax = file_read (f->file, buffer, size);
    }
    lock_release (&file_lock);
  }
  else if(args[0] == SYS_WRITE) {
    lock_acquire (&file_lock);
    int fd = args[1];
    const void *buffer = (const void *) args[2];
    unsigned size = args[3];
    if (fd == 1) {
      putbuf (buffer, size);
      f->eax = size;
    } 
    else {
      struct fnode *f = get_file_from_fd (fd); 
      if (f == NULL) 
        f->eax = -1; 
      else
        f->eax = file_write (f->file, buffer, size);
    }
    lock_release(&file_lock);
  }
  else if(args[0] == SYS_SEEK) {
    lock_acquire (&file_lock);
    int fd = args[1];
    struct fnode *f = get_file_from_fd (fd);
    if (f == NULL)
      f->eax = -1;
    else
      file_seek (f->file, args[2]);
    lock_release (&file_lock);
  }
  else if(args[0] == SYS_TELL) {
    lock_acquire (&file_lock);
    int fd = args[1];
    struct fnode *f = get_file_from_fd (fd);
    if (f == NULL)
      f->eax = -1; 
    else
      f->eax = file_tell (f->file);
    lock_release (&file_lock);
  }
  else if(args[0] == SYS_CLOSE) {
    lock_acquire (&file_lock);
    int fd = args[1];
    struct fnode *f = get_file_from_fd (fd);
    if (f == NULL)
      f->eax = -1;
    else {
      file_close (f->file);
      list_remove (f->elem); /* Once the file is closed, it is not in the file_list. */
    }
    lock_release (&file_lock);
  }
}

struct pnode *get_child_pnode (pid_t pid)
{
  struct list *list = thread_current ()->children;
  struct list_elem *e = list_begin (list);
  for (; e != list_end (list)); e = list_next (e)) {
    struct pnode *p = list_entry (e, struct pnode, elem);
    if (p->pid == pid)
      return p;
  }
  return NULL;
}

struct fnode* get_file_from_fd(int fd) {
  struct thread* t = thread_current ();
  struct list_elem *e;
  for(e = list_begin (&t->file_list); e != list_end (&t->file_list); e = list_next (e)) {
    struct fnode = list_entry(e, struct fnode, elem);
    if (fd == fnode->fd)
      return fnode;
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
