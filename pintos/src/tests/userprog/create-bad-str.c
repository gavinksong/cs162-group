#include "tests/lib.h"
#include "tests/main.h"

static char *PHYS_BASE = (char *) 0xC0000000;

void
test_main (void) 
{
  char *name = PHYS_BASE - 1;
  *name = 'c';
  create (name, 0);
  fail ("should have called exit(-1)");
}
