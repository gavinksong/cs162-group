#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  wait (exec ("create-bad-str"));
  open ("sample.txt");
}
