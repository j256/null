#!/bin/sh
#
# Test shell script
#

set -e

##################################################################
# -f write file tests
##################################################################

echo "Checking write output files..."
rm -f x.t y.t z.t

cat *.[ch] > x.t
# we should see the size of x.t in the output
./null -v -f y.t -f z.t x.t 2>&1 | grep `cat x.t | wc -c`

# output files should be the same
cmp x.t y.t
cmp x.t z.t

rm -f x.t y.t z.t
echo ""

##################################################################
# -m md5 signature tests
##################################################################

echo "Checking -m md5 argument..."
# that md5 signature is the empty string
./null -m 2>&1 /dev/null | grep "d41d8cd98f00b204e9800998ecf8427e"
echo ""

##################################################################
# -s stop-after tests
##################################################################

echo "Checking stop after -s argument..."
# should only see output of 100000 bytes
cat *.[ch] | ./null -v -s 100000 2>&1 | grep 100000
echo ""

##################################################################
# --help and --usage tests
##################################################################

echo "Checking --help and --usage..."
./null --help 2>&1 | grep "Null Utility:"
./null --usage 2>&1 | grep -- --help
echo ""

##################################################################
# -t throttle and rate tests
##################################################################

echo "Checking throttle and rate tests..."
# should take more than a second because throttling and should see rate info
cat *.[ch] | ./null -b 10k -s 100k -t 50k -R 1 2>&1 | grep "Writing at"
echo ""

##################################################################
# -r and -w read and write pagination tests
##################################################################

echo "Checking read and write pagination..."
# should not get error because tar worked
tar -czf - . | ./null -w -p | ./null -rpv | tar -tzvf - > /dev/null
echo ""
