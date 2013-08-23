#!/bin/sh

# do_tests.sh: Run all available tests.
#
# Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
# Copyright:	(c) 2013 DNW German-Dutch Windtunnels
# Created:	2013-08-23
# Version:	$Id: do_tests.sh 188 2013-08-23 06:50:59Z jacco $

errors=0

for f in `grep -l '#ifdef TEST' *.c`; do
    base=`basename "$f" .c`
    exe="test-$base"

    if gcc -std=c99 -D_GNU_SOURCE -DTEST -g -Wall -o $exe $f libjvs.a -lm; then
        ./$exe
        errors=`expr $errors + $?`
    else
        errors=`expr $errors + 1`
    fi
done

exit $errors
