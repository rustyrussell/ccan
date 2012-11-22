#! /bin/sh
# Script to list all files, for making tarballs.

set -e
if [ $# -eq 0 ]; then
    echo Usage: list_files.sh '<ccandir>...' >&2
    exit 1
fi

for d; do
    # git ls-files recurses, but we want ignores correct :(
    for f in `git ls-files $d | sed "s,^\($d/[^/]*\)/.*,\1," | uniq`; do
        # Include subdirs, unless it's a separate module.
	if [ -d "$f" ]; then
	    if [ ! -f "$f"/_info ]; then
		$0 "$f"
	    fi
	else
	    echo "$f"
	fi
    done
done
