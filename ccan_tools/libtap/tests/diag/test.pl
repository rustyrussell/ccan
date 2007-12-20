#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

plan tests => 2;

diag("A diagnostic message");

ok(1, 'test 1') or diag "ok() failed, and shouldn't";
ok(0, 'test 2') or diag "ok() passed, and shouldn't";
