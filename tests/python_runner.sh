#!/bin/bash

# Ensure a Python script argument was provided
if [ -z "$1" ]; then
	echo "Error: No Python script provided."
	echo "Usage: $0 <path_to_python_script> [additional_arguments...]"
	exit 1
fi

SCRIPT_TO_RUN="$1"
shift # Remove the script name from the argument list

# Detect the available Python executable
if command -v python &> /dev/null; then
	PYTHON_CMD="python"
elif command -v python3 &> /dev/null; then
	PYTHON_CMD="python3"
else
	echo "Error: Neither 'python' nor 'python3' could be found on this system."
	exit 1
fi

# Execute the Python script with any remaining arguments
"$PYTHON_CMD" "$SCRIPT_TO_RUN" "$@"

