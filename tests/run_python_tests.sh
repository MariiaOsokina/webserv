#!/bin/bash

if command -v python3 >/dev/null; then
	cmd="python3"
elif command -v python >/dev/null; then
	cmd="python"
else
	cmd=""
fi

if [ ! -z "$cmd" ]; then
	$cmd tests/test.py
else
	echo 'Error: no python3 or python command found!'
	exit 1
fi
