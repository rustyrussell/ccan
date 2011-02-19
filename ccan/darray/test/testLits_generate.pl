#!/usr/bin/perl
use strict;
use warnings;

my $amount = 10;
my $maxLen = 509;
srand(0);

my $templateFile = 'testLits.h.template';
my $outFile = 'testLits.h';

open(TF, $templateFile);

open(OUT, '>'.$outFile);
select OUT;

my $inLoop = 0;
my $loopText = '';

foreach my $line (<TF>) {
	$line =~ s/\@amount/$amount/g;
	if (!$inLoop) {
		if ($line =~ /\@forEachRandomString/) {
			$inLoop = 1;
			next;
		}
		print $line;
	} elsif ($inLoop == 1) {
		if ($line =~ /\@end/) {
			$inLoop = 0;
			#handle $loopText
			for (my $i=0; $i<$amount; $i++) {
				my $str = randomCString($maxLen);
				my $lt = $loopText;
				$lt =~ s/\@i/$i/g;
				$lt =~ s/\@str/\"$str\"/g;
				print "$lt\n";
			}
			$loopText = '';
			next;
		}
		$loopText .= $line;
	}
}

close(OUT);
close(TF);

#argument:  maxLen
sub randomCString {
	my $len = int(rand($_[0]+1));
	my $lastWasHex = 0;
	my $str = '';
	
	for (my $i=0; $i<$len; $i++) {
		my $cn = int(rand(255)) + 1;
		my $c = chr($cn);
		if ($lastWasHex && ($c =~ /[0-9A-Fa-f]/)) {
			$lastWasHex = 1;
			$str .= sprintf("\\x%02X", $cn);
		} elsif ($c =~ /[\t\n\013\f\r]/) {
			$lastWasHex = 0;
			$c =~ tr/\t\n\013\f\r/tnvfr/;
			$str .= '\\'.$c;
		} elsif ($cn<32 || $cn>126) {
			$lastWasHex = 1;
			$str .= sprintf("\\x%02X", $cn);
		} else {
			$lastWasHex = 0;
			if ($c =~ /[\"\\]/) {
				$str .= '\\'.$c;
			} else {
				$str .= $c;
			}
		}
	}
	return $str;
}
