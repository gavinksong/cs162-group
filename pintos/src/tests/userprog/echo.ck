# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_USER_FAULTS => 1, [<<'EOF']);
(echo) begin
Hello, CS162!
(echo) end
echo: exit(0)
EOF
pass;
