#!/bin/sh
(
    flawfinder -DQSI -m 0 "$@" | tee flaws.txt
	rats -i -lc -w 3 "$@"| tee -a flaws.txt
) | grep '.\+:[0-9]\+'
