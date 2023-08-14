#!/bin/sh

# run_tests.sh: Run all available tests.
#
# Copyright: (c) 2021-2023 Jacco van Schaik (jacco@jaccovanschaik.net)
# Created:   2021-01-21 20:28:57.352668070 +0100
# Version:   $Id: run_tests.sh 438 2021-08-19 10:10:03Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

errors=0

for exe in $*; do
    if ./$exe; then
        : # echo "$exe ok."
    else
        errors=`expr $errors + $?`
        echo "$exe failed."
    fi
done 

exit $errors
