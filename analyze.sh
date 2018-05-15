#!/bin/sh

type cppcheck && cppcheck \
	--enable=all \
	--std=c99 \
	--inconclusive \
	--quiet \
	-I inc/ \
	tide.c lib/*.c
