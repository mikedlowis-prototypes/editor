#!/bin/bash
gcov -fabc "$@" | pcregrep -vM '\nLines executed:100.00%(.*\n){3}'