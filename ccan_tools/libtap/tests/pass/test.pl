#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

plan tests => 2;

$rc = pass('test to pass');
diag("Returned: $rc");

$rc = pass('test to pass with extra string');
diag("Returned: $rc");
