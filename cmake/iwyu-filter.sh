#!/bin/sh
# Wrapper around include-what-you-use (see cmake/StcIwyu.cmake).
#
# Usage: iwyu-filter.sh <include-what-you-use> <its arguments...>
#
# Runs IWYU and cleans up its report:
#
# 1. Drops ONE known-harmless diagnostic:
#      /usr/include/c++/15/bits/mofunc_impl.h:1:1: warning: no private include name for @headername mapping
#    IWYU 0.23 - 0.25 prints it for libstdc++ headers it cannot resolve an include name for
#    (bits/mofunc_impl.h, bits/unicode-data.h); it was removed in IWYU 0.26.
#
# 2. Removes suggestions listed in iwyu-ignore.txt (template-internals of Qt, libstdc++ false
#    positives) via iwyu-filter.awk. A file whose only suggestions were ignored disappears from
#    the report altogether.
#
# IWYU's exit code is preserved - except that it becomes 0 when nothing is left to report, because
# CMake shows IWYU's output only when the exit code is non-zero. STC_IWYU_IGNORE_FILE overrides
# the ignore list.

dir=$(cd "$(dirname "$0")" && pwd)
ignore="${STC_IWYU_IGNORE_FILE:-$dir/iwyu-ignore.txt}"

base=$(mktemp "${TMPDIR:-/tmp}/stc-iwyu.XXXXXX") || exec "$@"
raw="$base.raw"; out="$base.out"

"$@" 2>"$raw"
rc=$?

grep -v -E ': warning: no private include name for @headername mapping$' "$raw" > "$base"

if [ "$rc" -ne 0 ] && [ -r "$ignore" ] && [ -r "$dir/iwyu-filter.awk" ]; then
    if command -v awk >/dev/null 2>&1; then
        awk -v ignore="$ignore" -f "$dir/iwyu-filter.awk" "$base" > "$out" 2> "$base.err"
        ar=$?
    else
        ar=127
        echo "awk: command not found" > "$base.err"
    fi
    case $ar in
        0)  rc=0 ;;                      # nothing left to report
        10) cat -s "$out" >&2 ;;
        *)  # awk problem: say so, then show the unfiltered report
            echo "iwyu-filter.sh: awk failed (exit $ar): $(head -n 1 "$base.err") - showing the unfiltered IWYU report" >&2
            cat "$base" >&2 ;;
    esac
    rm -f "$base.err"
else
    if [ "$rc" -ne 0 ]; then
        [ -r "$ignore" ] || echo "iwyu-filter.sh: ignore list not readable: $ignore - report not filtered" >&2
        [ -r "$dir/iwyu-filter.awk" ] || echo "iwyu-filter.sh: $dir/iwyu-filter.awk not readable - report not filtered" >&2
    fi
    cat "$base" >&2
fi

rm -f "$base" "$raw" "$out"
exit "$rc"
