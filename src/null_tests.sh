#!/bin/sh
#
# Test shell script
#

set -e

##################################################################
# -f write file tests
##################################################################

rm -f x.t y.t z.t

cat *.[ch] > x.t
# we should see the size of x.t in the output
./null -v -f y.t -f z.t x.t 2>&1 | grep `cat x.t | wc -c`

# output files should be the same
cmp x.t y.t
cmp x.t z.t

rm -f x.t y.t z.t

##################################################################
# -m md5 signature tests
##################################################################

# that md5 signature is the empty string
./null -m 2>&1 /dev/null | grep "d41d8cd98f00b204e9800998ecf8427e"

##################################################################
# -s stop-after signature tests
##################################################################

# should only see output of 100000 bytes
cat *.[ch] | ./null -v -s 100000 2>&1 | grep 100000

##################################################################
# --help and --usage tests
##################################################################

./null --help 2>&1 | grep "Null Utility:"
./null --usage 2>&1 | grep -- --help

##################################################################
# -t throttle and rate tests
##################################################################

# should take more than a second because throttling and should see rate info
cat *.[ch] | ./null -b 10k -s 100k -t 50k -R 1 2>&1 | grep "Writing at"

##################################################################
# -r and -w read and write pagination tests
##################################################################

# should not get error because tar worked
tar -czf - . | ./null -w -p | ./null -rpv | tar -tzvf - > /dev/null
