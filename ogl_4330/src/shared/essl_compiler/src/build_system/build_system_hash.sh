#!/bin/bash
#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2009-2010 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

#
# Hash a build directory to a shorted name, for systems with a path length
# In addition, create appropriate symlinks for compatibility with the user.
#
# This does not work atomically.
#

function usage()
{
	echo "Usage: $0 build_root dir_string" 1>&2
	echo "build_root will be created if necessary" 1>&2
	echo "dir_string will be created, hashed, and a symlink named dir_string will link to it for compatibility" 1>&2
	echo "" 1>&2
}

if [ -z "$1" -o -z "$2" ] ; then
	usage
	exit 1
fi

BUILD_ROOT="$1"
DIR_STRING="$2"

mkdir -p "$BUILD_ROOT"

MAX_ATTEMPTS=255
ATTEMPT=0
HASHED_DIR_BASE=$(echo "$DIR_STRING" | md5sum | cut -d ' ' -f 1)
HASHED_DIR="$HASHED_DIR_BASE.$ATTEMPT"

HASHED_DIR_COLLISIONS=$(ls -df "$BUILD_ROOT/$HASHED_DIR_BASE."* 2>/dev/null || true)
if [ -z "$HASHED_DIR_COLLISIONS" ] ; then
	# First creation
	mkdir -p "$BUILD_ROOT/$HASHED_DIR"
	echo "$DIR_STRING">"$BUILD_ROOT/$HASHED_DIR/build_system_hash"
	ln -s "$HASHED_DIR" "$BUILD_ROOT/$DIR_STRING"
	echo "$HASHED_DIR"
	exit 0
fi

# Otherwise, search for the correct hash
for dir in $HASHED_DIR_COLLISIONS ; do
	if [ "$(cat ${dir}/build_system_hash )" == "$DIR_STRING" ] ; then
		# found!
		echo "${dir##$BUILD_ROOT/}"
		exit 0
	fi
done

# Otherwise, attempt to create one
for i in $(seq 0 $MAX_ATTEMPTS) ; do
	HASHED_DIR="$HASHED_DIR_BASE.$i"
	if [ ! -d "$BUILD_ROOT/$HASHED_DIR"  ] ; then
		# Found a gap
		mkdir -p "$BUILD_ROOT/$HASHED_DIR"
		echo "$DIR_STRING">"$BUILD_ROOT/$HASHED_DIR/build_system_hash"
		ln -s "$HASHED_DIR" "$BUILD_ROOT/$DIR_STRING"
		echo "$HASHED_DIR"
		exit 0
	fi
done

# Otherwise, failure

echo "Too many collisions ($MAX_ATTEMPTS) for $HASHED_DIR_BASE, aborting" 1>&2
# Fail-safe (sort of)
echo "$DIR_STRING"
exit 1
