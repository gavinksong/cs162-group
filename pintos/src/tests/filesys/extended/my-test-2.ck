# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(my-test-2) begin
(my-test-2) create "a"
(my-test-2) open "a"
(my-test-2) write 100 kB to "a"
(my-test-2) called block_read 1 times in 200 writes
(my-test-2) called block_write 201 times in 200 writes
(my-test-2) close "a"
(my-test-2) end
EOF
pass;
