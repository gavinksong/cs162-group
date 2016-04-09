/* Tests reading from stdin. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  char msg[64];
  size_t msglen = read(0, msg, 64);
  write(1, msg, msglen);
}
