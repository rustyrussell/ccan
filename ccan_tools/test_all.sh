#! /bin/sh

# First, test normal config.
if ! make -s; then
    echo Normal config failed.
    exit 1
fi

# Now, remove one HAVE_ at a time.
cp config.h original-config.h
trap "mv original-config.h config.h && rm -f .newconfig" EXIT

while grep -q '1$' config.h; do
    tr '\012' @ < config.h | sed 's/1@/0@/' | tr @ '\012' > .newconfig
    diff -u config.h .newconfig
    mv .newconfig config.h
    if ! make -s; then
	echo Failed config:
	cat config.h
	exit 1
    fi
done
