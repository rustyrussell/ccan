#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

plan tests => 2;

$rc = fail('test to fail');
diag("Returned: $rc");

$rc = fail('test to fail with extra string');
diag("Returned: $rc");
