#!/bin/sh

# do_tests.sh: Run all available tests.
#
# Author:       Jacco van Schaik (jacco.van.schaik@dnw.aero)
# Copyright:    (c) 2013 DNW German-Dutch Windtunnels
# Created:      2013-08-23
# Version:      $Id: do_tests.sh 384 2019-11-28 13:43:29Z jacco $

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
