/* Try reading a file in the most normal way.
And calcualte the cache-effectiveness*/
#include <random.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define BLOCK_SIZE 512
static char buf_a[BLOCK_SIZE];

void
test_main (void)
{
  int fd;
  size_t ret_val;
  random_init (0);
  random_bytes (buf_a, sizeof buf_a);
  CHECK (create ("a", 0), "create \"tmp\"");
  CHECK ((fd = open ("a")) > 1, "open \"tmp\"");

  msg("creating tmp file");
  int i = 0;
  for(;i < 64; i++)
  {
    ret_val = write (fd, buf_a, BLOCK_SIZE);
    if (ret_val != BLOCK_SIZE)
      fail ("write %zu bytes in \"tmp\" returned %zu",
            BLOCK_SIZE, ret_val);
  }
  msg ("close \"tmp\"");
  close (fd);

  msg("resetting buffer");
  buffer_reset ();

  msg("open and read tmp");
  for (i = 0; i < 64; i ++)
    read (fd, buf_a, BLOCK_SIZE);
  close (fd);
  msg("close tmp");

  int old_hit = buffer_stat (1);
  int old_total = buffer_stat (0) + old_hit;
  msg ("Hit rate of the first reading: %d", old_hit/old_total);

  msg("open and read tmp again");
  fd = open ("shakespeare.txt");
  for (i = 0; i < 64; i ++)
    read (fd, buf_a, 512);
  close (fd);
  msg("close tmp");

  int new_hit = buffer_stat (1) ;
  int new_total = buffer_stat (0) + new_hit;
  msg("new_total %d %d %d %d", old_hit, old_total, new_hit, new_total);
  msg ("Hit rate of the second reading: %d", (new_hit-old_hit)/(new_total-old_total));
}
