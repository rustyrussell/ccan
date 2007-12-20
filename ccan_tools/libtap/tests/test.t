#!/bin/sh

echo '1..2'

make 2>&1 > /dev/null || exit 1

# Comment this out if you care about exact formatting
rationalize()
{
    tr -s ' ' | sed -e 's/ tests / test /' -e "s/ test '[^']*'/ test ()/" -e "s/ test (.*)/ test ()/" -e 's, Second plan at \./test.pl line.*,,' -e 's,Failed test in ./test.pl at line .*,Failed test (),'
}

perl ./test.pl 2>&1 | rationalize | grep -v '^# \+in ./test.pl at line'> test.pl.out
perlstatus=$?

./test 2>&1 | rationalize > test.c.out
cstatus=$?

ret=0
diff -u test.pl.out test.c.out

if [ $? -eq 0 ]; then
	echo 'ok 1 - output is identical'
else
	echo 'not ok 1 - output is identical'
	ret=1
fi

if [ $perlstatus -eq $cstatus ]; then
	echo 'ok 2 - status code'
else
	echo 'not ok 2 - status code'
	echo "# perlstatus = $perlstatus"
	echo "#    cstatus = $cstatus"
	ret=1
fi

exit $ret
