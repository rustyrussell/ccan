#!/usr/bin/perl

use warnings;
use strict;

my $rc = 0;

use Test::More;

plan qw(no_plan);

$rc = ok(1);
diag("Returned: $rc");
