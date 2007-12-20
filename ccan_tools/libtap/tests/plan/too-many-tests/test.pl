#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

plan tests => 5;

$rc = ok(1);
diag("Returned: $rc");

$rc = ok(0);
diag("Returned: $rc");
