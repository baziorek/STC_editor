#!/bin/sh
# Wrapper around include-what-you-use (see cmake/StcIwyu.cmake).
#
# Usage: iwyu-filter.sh <include-what-you-use> <its arguments...>
#
# Runs IWYU unchanged and drops ONE known-harmless diagnostic from its output:
#
#   /usr/include/c++/15/bits/mofunc_impl.h:1:1: warning: no private include name for @headername mapping
#
# IWYU 0.23 - 0.25 prints it for libstdc++ headers it cannot resolve an include name for
# (bits/mofunc_impl.h, bits/unicode-data.h - both are mapped by hand in iwyu.imp); the message
# was removed in IWYU 0.26. It only adds a screenful of noise to every report.
#
# IWYU's exit code is preserved: CMake shows IWYU's output only when it is non-zero.

tmp=$(mktemp "${TMPDIR:-/tmp}/stc-iwyu.XXXXXX") || exec "$@"
"$@" 2>"$tmp"
rc=$?
grep -v -E ': warning: no private include name for @headername mapping$' "$tmp" >&2
rm -f "$tmp"
exit "$rc"
