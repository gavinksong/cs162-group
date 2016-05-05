/* Try reading a file in the most normal way. 
And calcualte the cache-effectiveness*/

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int fd = open ("");

  char block[512];
  buffer_reset ();
  int i;
  for (i = 0; i < 64; i ++)
    read (fd, block, 512);
  close (fd);
  int old_hit = buffer_stat (1);
  int old_total = buffer_stat (0) + buffer_stat (1);
  msg ("Hit rate of the first reading: %d", old_hit/old_total);
  fd = open ("shakespeare.txt"); 
  for (i = 0; i < 64; i ++)
    read (fd, block, 512);
  close (fd);
  int new_hit = buffer_stat (1) - old_hit;
  int new_total = buffer_stat (0) + buffer_stat (1) - old_total;
  msg ("Hit rate of the second reading: %d", new_hit/new_total);
}