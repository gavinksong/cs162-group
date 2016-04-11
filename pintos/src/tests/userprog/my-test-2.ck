# -​*- perl -*​-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(my-test-2) begin
(create-bad-str) begin
create-bad-str: exit(-1)
(my-test-2) end
my-test-2: exit(0)
EOF
pass;