#!/bin/sh
# Stands in for RemoteGenNG: counts its runs, copies list.txt to the file of --list-dependencies and writes the <src>
# directory of the configuration. Marker files next to it or in CMAKE_CURRENT_SOURCE_DIR change what it does.
list=
source=
for argument; do
	case $argument in
	--help)
		echo "help" >> "$(dirname "$0")/help-runs.txt"
		if [ -e "$(dirname "$0")/help-signal" ]; then
			kill -KILL $$
		fi
		echo "usage: remotegen.sh [<option> ...] <file> ..."
		if [ ! -e "$(dirname "$0")/no-list" ]; then
			echo "--list-dependencies=<file>"
		fi
		exit 0;;
	--list-dependencies=*)
		list=${argument#--list-dependencies=};;
	-DCMAKE_CURRENT_SOURCE_DIR=*)
		source=${argument#-DCMAKE_CURRENT_SOURCE_DIR=};;
	esac
	config=$argument
done
if [ -z "$source" ]; then
	echo "remotegen.sh: the property CMAKE_CURRENT_SOURCE_DIR is not set" >&2
	exit 1
fi
if [ -n "$list" ]; then
	echo "list" >> "$source/lists.txt"
	if [ -e "$source/list-silent" ]; then
		exit 0
	fi
	if [ -e "$source/list.txt" ]; then
		cp "$source/list.txt" "$list"
	else
		: > "$list"
	fi
	if [ -e "$source/list-fail" ]; then
		echo "remotegen.sh: the list run fails" >&2
		exit 1
	fi
	exit 0
fi
out=$(sed -n 's:.*<src>\(.*\)/src</src>.*:\1:p' "$config")
if [ -e "$out" ]; then
	echo "remotegen.sh: the directory $out exists" >&2
	exit 1
fi
mkdir -p "$out/src"
if [ -e "$source/fail" ]; then
	echo "// partial" > "$out/src/Service.cpp"
	exit 1
fi
if [ -e "$source/change-header" ]; then
	rm "$source/change-header" && touch "$source/base/include/Service.h"
fi
if [ -e "$source/extra-output" ]; then
	echo "// extra" > "$out/src/Extra.cpp"
fi
if [ -e "$source/non-ascii-output" ]; then
	echo "// non-ASCII" > "$out/src/$(printf '\303\204').cpp"
fi
content="// generated"
if [ -e "$source/changed-output" ]; then
	content="// changed"
fi
service=Service.cpp
if [ -e "$source/upper-case-output" ]; then
	service=SERVICE.cpp
fi
echo "$content" > "$out/src/$service" && echo "$out" >> "$source/runs.txt"
