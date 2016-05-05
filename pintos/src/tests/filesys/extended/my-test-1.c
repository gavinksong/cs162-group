/* Try reading a file in the most normal way.
And calcualte the cache-effectiveness*/
#include <random.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define BLOCK_SIZE 512
#define NUM_BLOCKS 50
static char buf_a[BLOCK_SIZE];

void
test_main (void)
{
  int fd;
  size_t ret_val;
  random_init (0);
  random_bytes (buf_a, sizeof buf_a);
  CHECK (create ("a", 0), "create \"a\"");
  CHECK ((fd = open ("a")) > 1, "open \"a\"");

  msg("creating a");
  int i = 0;
  for(; i < NUM_BLOCKS; i++)
  {
    ret_val = write (fd, buf_a, BLOCK_SIZE);
    if (ret_val != BLOCK_SIZE)
      fail ("write %zu bytes in \"a\" returned %zu",
            BLOCK_SIZE, ret_val);
  }
  msg ("close \"tmp\"");
  close (fd);

  msg("resetting buffer");
  buffer_reset ();

  CHECK ((fd = open ("a")) > 1, "open \"a\"");
  msg ("read tmp");
  for (i = 0; i < NUM_BLOCKS; i++)
  {
    ret_val = read (fd, buf_a, BLOCK_SIZE);
    if (ret_val != BLOCK_SIZE)
        fail ("read %zu bytes in \"a\" returned %zu",
              BLOCK_SIZE, ret_val);
  }
  close (fd);
  msg("close \"a\"");

  int old_hit = buffer_stat (1);
  int old_total = buffer_stat (0) + old_hit;
  int old_hit_rate = (100 * old_hit) / old_total;

  CHECK ((fd = open ("a")) > 1, "open \"a\"");
  msg ("read \"a\"");
  for (i = 0; i < NUM_BLOCKS; i ++)
  {
    ret_val = read (fd, buf_a, BLOCK_SIZE);
    if (ret_val != BLOCK_SIZE)
        fail ("read %zu bytes in \"a\" returned %zu",
              BLOCK_SIZE, ret_val);
  }
  close (fd);
  msg("close \"a\"");

  remove ("a");


  int new_hit = buffer_stat (1) ;
  int new_total = buffer_stat (0) + new_hit;
  int new_hit_rate = 100 * (new_hit - old_hit) / (new_total - old_total);

  if (new_hit_rate>old_hit_rate){
    msg("Hit rate of the second reading is greater than hit rate of the first reading");
  }
}
