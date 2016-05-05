# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(cache-effectiveness) begin
Hit rate of the first reading: 100.0
Hit rate of the second reading: 100.0
(cache-effectiveness) end
cache-effectiveness: exit(0)
EOF
pass;