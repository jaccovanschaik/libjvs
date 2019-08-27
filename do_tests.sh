#!/bin/sh

# do_tests.sh: Run all available tests.
#
# Author:       Jacco van Schaik (jacco.van.schaik@dnw.aero)
# Copyright:    (c) 2013 DNW German-Dutch Windtunnels
# Created:      2013-08-23
# Version:      $Id: do_tests.sh 343 2019-08-27 08:39:24Z jacco $

errors=0

for f in `grep -l '#ifdef TEST' *.c`; do
    exe=`echo "$f" | sed 's/\.c/\.test/'`

    if make --no-print-directory $exe && ./$exe; then
        echo "$exe ok."
    else
        errors=`expr $errors + $?`
        echo "$exe failed."
    fi
done

exit $errors
