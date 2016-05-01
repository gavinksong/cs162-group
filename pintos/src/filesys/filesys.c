#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);
static int get_next_part (char part[NAME_MAX + 1], const char **srcp);
static bool follow_path (const char *path, struct inode **);
static struct dir* get_parent_dir(const char *path);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = get_parent_dir(name);
  char *filename = malloc(sizeof(char) * (NAME_MAX + 1 ));
  int read;
  while ((read = get_next_part (filename, &name)) > 0) {
  }

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, filename, inode_sector));
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
  struct inode *save_inode = inode_malloc();
  bool success = follow_path(name, &save_inode);
  struct inode_disk *parent_disk = inode_get_parentdisk(save_inode);
  
  struct file *result;
  if (success && inode_get_isdir(save_inode))
    result = (struct file *)dir_open(save_inode);
  else
    result = file_open (save_inode);

  if (result)
    increment_file_cnt(parent_disk);
  return result;

}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;
  struct inode *save_inode = inode_malloc();
  bool success = follow_path(name, &save_inode);
  char *filename = malloc(sizeof(char) * (NAME_MAX + 1 ));
  struct inode_disk *parent_disk = inode_get_parentdisk(save_inode);
  struct inode *parent_inode = inode_open(inode_get_parentsector(save_inode));
  int read;
  while ((read = get_next_part (filename, &name)) > 0) {
  }
  if (success)
    dir = dir_open(parent_inode);
  if (success && read == 0 &&
      ((inode_get_isdir (save_inode) && inode_get_sector(save_inode) != ROOT_DIR_SECTOR 
        && inode_get_file_cnt(save_inode) == 0) 
      || !inode_get_isdir(save_inode))){
    success = dir_remove(dir, filename);
    dir_close(dir);
  }
  else
    success = false;
  

  if(success)
    decrement_file_cnt(parent_disk);

  return success;
}

bool filesys_chdir(const char *path, struct inode **inode) {
  bool success = follow_path (path, inode);
  return success && inode_get_isdir(*inode);
}

struct inode *get_file_inode(struct file *file) {
  return file_get_inode (file);
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

static struct dir* 
get_parent_dir(const char *path) {
  struct inode *start_inode = NULL;
  struct inode *save_inode = NULL;
  struct dir *dir = NULL;
  char *part = malloc(sizeof(char) * (NAME_MAX + 1 ));
  int read = get_next_part (part, &path);
  if (read == -1)
    return NULL;
  if(read == 1 && strcmp(part, "..") == 0) {
    struct inode *cur_inode = thread_current()->cwd;
    block_sector_t parent_sector = inode_get_parentsector(cur_inode);
    start_inode = inode_open (parent_sector);
  } else if (read == 1 && strcmp(part, "/")  == 0) {
    start_inode = dir_get_inode (dir_open_root());
  } else {
    start_inode = thread_current ()->cwd;
  }
  read = get_next_part(part, &path);
  while (read > 0)
  {
    dir = dir_open(start_inode);
    bool lookup = dir_lookup(dir, part, &save_inode);
    dir_close(dir);
    read = get_next_part(part, &path);
    if (read == -1)
      return NULL;
    if (!lookup && read != 0)
      break;
    else if(read == 0)
      return dir;
    start_inode = save_inode;
  }
  return NULL;
}

/* Stores the inode corresponding to the path within INODE. 
   Returns false if the path is invalid. */
static bool 
follow_path (const char *path, struct inode **inode)
{
  struct dir *parent_dir = get_parent_dir(path);
  if (parent_dir == NULL)
    return false;
  char *filename = malloc(sizeof(char) * (NAME_MAX + 1 ));
  int read;
  while ((read = get_next_part (filename, &path)) > 0) {
  }
  if (read == -1)
    return false;
  bool lookup = dir_lookup(parent_dir, filename, inode);
  if (!lookup)
    return false;
  return true;
}

static int
get_next_part (char part[NAME_MAX + 1], const char **srcp) {
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
