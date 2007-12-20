#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

plan tests => 3;

$rc = ok(1, 'First test');
diag("Returned: $rc");

$rc = ok(1, '1');
diag("Returned: $rc");

$rc = ok(1, 'Third test');
diag("Returned: $rc");
