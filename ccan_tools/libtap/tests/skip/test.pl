#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $rc = 0;

plan tests => 8;

my $side_effect = 0;		# Check whether skipping has side effects

$rc = ok(1 == 1, '1 equals 1');	# Test ok() passes when it should
diag("Returned: $rc");

# Start skipping
SKIP: {
	skip "Testing skipping", 1;

	$side_effect++;

	$rc = ok($side_effect == 1, 'side_effect checked out');
}

SKIP: {
	skip "Testing skipping #2", 1;

	$side_effect++;

	$rc = ok($side_effect == 1, 'side_effect checked out');
	diag("Returned: $rc");
}

$rc = ok($side_effect == 0, "side_effect is $side_effect");
diag("Returned: $rc");

SKIP: {
	if (1 == 1) {
		skip "Testing skip_if", 1;
	}

	$side_effect++;

	$rc = ok($side_effect == 1, 'side_effect checked out');
	diag("Returned: $rc");
}

$rc = ok($side_effect == 0, "side_effect is $side_effect");
diag("Returned: $rc");

SKIP: {
	if (1 == 0) {
		skip "Testing skip_if #2", 1;
	}

	$side_effect++;

	$rc = ok($side_effect == 1, 'side_effect checked out');
	diag("Returned: $rc");
}

$rc = ok($side_effect == 1, "side_effect is $side_effect");
diag("Returned: $rc");
