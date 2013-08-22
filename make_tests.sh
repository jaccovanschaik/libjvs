#!/bin/sh

# make_tests.sh: Description
#
# Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: make_tests.sh 186 2013-08-22 19:42:34Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

rule="test:"

for f in `grep -l '#ifdef TEST' *.c`; do
    base=`basename "$f" .c`
    exe="test-$base"
    rule="$rule $exe"
    echo "$exe: $f libjvs.a"
    echo '\t$(CC) $(CFLAGS) -DTEST -o $@ $^ -lm'
    echo "\t./$exe"
    echo ''
done

echo "$rule"
