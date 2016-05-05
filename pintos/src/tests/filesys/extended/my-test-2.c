/* Grows two files in parallel and checks that their contents are
   correct. */

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
  int n_read1;
  int n_read2;
  int n_write1;
  int n_write2;
  size_t ret_val;
  random_init (0);
  random_bytes (buf_a, sizeof buf_a);
  CHECK (create ("a", 0), "create \"a\"");
  CHECK ((fd = open ("a")) > 1, "open \"a\"");
  n_read1 = buffer_stat (2);
  n_write1 = buffer_stat (3);
  msg ("write 100 kB to \"a\"");
  int i = 0;
  for(;i < 200; i++)
  {
    ret_val = write (fd, buf_a, BLOCK_SIZE);
    if (ret_val != BLOCK_SIZE)
      fail ("write %zu bytes in \"a\" returned %zu",
            BLOCK_SIZE, ret_val);
  }
  n_read2 = buffer_stat (2);
  n_write2 = buffer_stat (3);
  msg("called block_read %d times in 200 writes", n_read2-n_read1);
  msg("called block_write %d times in 200 writes", n_write2-n_write1);
  msg ("close \"a\"");
  close (fd);
  remove("a");
}
