#! /bin/sh

# Compute the test dependencies for a ccan module. Usage:
# tools/gen_deps.sh ccan/path/to/module

path=$1
module=`echo $path | sed 's/^ccan\///g'`

# The test depends on the test sources ...
test_srcs=`ls $path/test/*.[ch] 2>/dev/null | tr '\n' ' '`

# ... and the object files of our module (rather than the sources, so
#     that we pick up the resursive dependencies for the objects)
module_objs=`ls $path/*.c 2>/dev/null | sed 's/.c$/.o/g' | tr '\n' ' '`

# ... and on the modules this test uses having passed their tests
deps=$(echo `$path/info depends` | tr ' ' '\n' | \
       sort | uniq | sed -e 's/$/\/.ok/g' -e '/^\/.ok$/d' | tr '\n' ' ')

# Print the test targets and target aliases
echo "${module}_ok_deps := $test_srcs $module_objs $deps"
echo "$path/.ok: \$(${module}_ok_deps)"
echo "$path/.fast-ok: \$(${module}_ok_deps:%.ok=%.fast-ok)"
