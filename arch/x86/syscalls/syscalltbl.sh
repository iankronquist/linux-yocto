#!/bin/sh

in="$1"
out="$2"
echo '-----------------------------------------------------------------' >&2
grep '^[0-9]' "$in" | sort -n | (
    while read nr abi name entry compat; do
	abi=`echo "$abi" | tr '[a-z]' '[A-Z]'`
	if [ -n "$compat" ]; then
	    echo "__SYSCALL_${abi}($nr, $entry, $compat)"
	    echo "__SYSCALL_${abi}($nr, $entry, $compat)" >&2
	elif [ -n "$entry" ]; then
	    echo "__SYSCALL_${abi}($nr, $entry, $entry)"
	    echo "__SYSCALL_${abi}($nr, $entry, $entry)" >&2
	fi
    done
) > "$out"
