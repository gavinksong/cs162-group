# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(my-test-1) begin
(my-test-1) create "a"
(my-test-1) open "a"
(my-test-1) creating a
(my-test-1) close "tmp"
(my-test-1) resetting buffer
(my-test-1) open "a"
(my-test-1) read tmp
(my-test-1) close "a"
(my-test-1) open "a"
(my-test-1) read "a"
(my-test-1) close "a"
(my-test-1) Hit rate of the second reading is greater than hit rate of the first reading
(my-test-1) end
my-test-1: exit(0)

EOF
pass;
