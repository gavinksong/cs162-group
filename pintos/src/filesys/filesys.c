#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer-cache.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);
static bool follow_path (const char *path, struct dir **, char filename[NAME_MAX +1]);
static int get_next_part (char part[NAME_MAX + 1], const char **srcp);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  buffer_cache_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  thread_current ()->cwd = inode_open (ROOT_DIR_SECTOR);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  buffer_cache_flush ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool isdir) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = NULL;
  char filename[NAME_MAX + 1];
  
  bool success = follow_path (name, &dir, filename)
                 && free_map_allocate (1, &inode_sector)
                 && inode_create (inode_sector, initial_size, isdir)
                 && dir_add (dir, filename, inode_sector);

  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = NULL;
  char filename[NAME_MAX + 1];
  struct inode *inode = NULL;

  if (follow_path (name, &dir, filename))
    dir_lookup (dir, filename, &inode);
  dir_close (dir);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name)
{
  struct dir *dir = NULL;
  char filename[NAME_MAX + 1];
  bool success = follow_path (name, &dir, filename)
                 && dir_remove (dir, filename);
  dir_close (dir);

  return success;
}

/* Changes the current working directory of the
  current thread to the directory located at PATH.
  Returns true is successful, false otherwise. */
bool
filesys_chdir (const char *path)
{
  struct thread *t = thread_current ();
  struct dir *dir = NULL;
  char filename[NAME_MAX + 1];
  struct inode *inode = NULL;
  
  bool success = follow_path (path, &dir, filename)
                 && dir_lookup (dir, filename, &inode);
  dir_close (dir);

  if (success) {
    inode_close (t->cwd);
    t->cwd = inode;
  }
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* Stores the name of the file referenced by PATH in
   FILENAME, and the directory in DIR. If the file is
   a directory, FILENAME is set to ".".
   Returns false if the path is invalid or an error occurs,
   true otherwise. */
static bool
follow_path (const char *path, struct dir **dir_,
             char filename[NAME_MAX +1])
{
  struct inode *inode;
  struct inode *next;

  /* Empty path name. */
  if (path[0] == '\0')
    return false;
  
  /* Absolute or relative path name. */
  if (path[0] == '/')
    inode = next = inode_open (ROOT_DIR_SECTOR);
  else
    inode = next = inode_reopen (thread_current ()->cwd);

  while (get_next_part (filename, &path) == 1) {
    struct dir *dir = dir_open (inode_reopen (inode));
    dir_lookup (dir, filename, &next);
    dir_close (dir);
    if (next == NULL || !inode_isdir (next))
      break;
    inode_close (inode);
    inode = next;
  }

  /* PATH was not parsed completely. */
  if (get_next_part (filename, &path) != 0)
    return false;

  /* If NEXT is a directory, set FILENAME to "." */
  if (inode == next)
    strlcpy (filename, ".", 2);
  else
    inode_close (next);

  return (*dir_ = dir_open (inode)) != NULL;
}

static int
get_next_part (char part[NAME_MAX + 1], const char **srcp)
{
  const char *src = *srcp;
  char *dst = part;
  /* Skip leading slashes. If it’s all slashes, we’re done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;
  /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
  while (*src != '/' && *src != '\0') {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++;
  }
  *dst = '\0';
  /* Advance source pointer. */
  *srcp = src;
  return 1;
}
