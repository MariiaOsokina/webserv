#!/bin/bash

#
# Usage: prep_www_for_delete_testing.sh [clean|fclean] [root_dir]
#
# clean|fclean        Will clean up the www/delete_test directory
#                     this is needed, because the delete_test has
#                     a read only directory that needs a chmod before
#                     the delete!
#
# root_dir            Directory containing the www folder
#                     If this is not given, it is assumed, that the
#                     current working directory contains www
#

DIR_NAME=www/delete_test
DO_CLEANUP=

if [ ! -z $1 ]; then
	if [ "$1" == "clean" ] || [ "$1" == "fclean" ]; then
		DO_CLEANUP=1
		shift
	fi
fi

if [ ! -z $1 ]; then
	if [ "$1" != "clean" ] || [ "$1" != "fclean" ]; then
		if [[ $1 =~ /$ ]]; then
			DIR_NAME="$1$DIR_NAME"
		else
			DIR_NAME="$1/$DIR_NAME"
		fi
		shift
	fi
fi

DIR_RD_ONLY="$DIR_NAME/read_only_dir"

setup_test_dir_for_delete_request() {

	clean_up_dir_for_delete_request

	mkdir "$DIR_NAME"

	mkdir "$DIR_RD_ONLY"
	echo "This is a read only file" >"$DIR_RD_ONLY/read_only_file"
	chmod 444 "$DIR_RD_ONLY/read_only_file"
	chmod 555 "$DIR_RD_ONLY"

	echo "Some content for this file" >>"$DIR_NAME/file1.txt"
	echo "Try delete me" >>"$DIR_NAME/file2.txt"
}

clean_up_dir_for_delete_request() {
	if [ -d "$DIR_NAME" ]; then
		chmod -R 755 "$DIR_NAME"
		rm -r "$DIR_NAME"
	fi
}

if [ $DO_CLEANUP ]; then
	clean_up_dir_for_delete_request
else
	setup_test_dir_for_delete_request
fi
