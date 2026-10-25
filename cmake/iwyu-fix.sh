#!/bin/sh
# Applies include-what-you-use's suggestions to the sources of STC_editor.
#
#   cmake/iwyu-fix.sh [-n] [-j N] <build-dir> [file ...]
#
#   <build-dir>  a configured build directory with compile_commands.json
#                (CMAKE_EXPORT_COMPILE_COMMANDS=ON; Qt Creator: Projects -> CMake -> add the flag),
#                the one where cmake/StcIwyu.cmake generated iwyu-qt.imp
#   file ...     sources to process (default: every .cpp of the application)
#   -n           dry run - only print the diffs
#   -j N         parallel IWYU jobs (default: number of CPUs)
#
# What it does, in order:
#   1. runs IWYU (iwyu_tool) with cmake/iwyu.imp + <build-dir>/iwyu-qt.imp
#   2. removes the suggestions listed in cmake/iwyu-ignore.txt (same filter as the build uses)
#   3. feeds the rest to fix_include (fix_includes.py), WITHOUT re-ordering existing includes
#      and without adding "// for ..." comments, so the diff only contains added/removed lines.
#
# Review with `git diff`, build, run the tests. Run it again afterwards: IWYU sometimes needs a
# second pass (e.g. after a header lost an include that its users relied on).

set -u
dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/.." && pwd)

die() { echo "iwyu-fix.sh: $*" >&2; exit 1; }
find_tool() { for t in "$@"; do if command -v "$t" >/dev/null 2>&1; then command -v "$t"; return 0; fi; done; return 1; }

dry=""
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
while [ $# -gt 0 ]; do
    case $1 in
        -n) dry="-n"; shift ;;
        -j) jobs=$2; shift 2 ;;
        -h|--help) sed -n '2,22p' "$0"; exit 0 ;;
        -*) die "unknown option $1" ;;
        *) break ;;
    esac
done
[ $# -ge 1 ] || die "usage: $0 [-n] [-j N] <build-dir> [file ...]"
build=$1; shift
[ -d "$build" ] || die "$build is not a directory"
build=$(cd "$build" && pwd)   # IWYU runs inside the build directory, so paths must be absolute
[ -f "$build/compile_commands.json" ] || die "$build/compile_commands.json not found (configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)"

iwyu_tool=$(find_tool iwyu_tool.py iwyu_tool) || die "iwyu_tool(.py) not found (sudo apt install iwyu)"
fix=$(find_tool fix_includes.py fix_include fix_includes) || die "fix_include / fix_includes.py not found (sudo apt install iwyu)"
command -v python3 >/dev/null 2>&1 || die "python3 not found"
command -v awk >/dev/null 2>&1 || die "awk not found"

maps="-Xiwyu --mapping_file=$dir/iwyu.imp"
[ -f "$build/iwyu-qt.imp" ] && maps="$maps -Xiwyu --mapping_file=$build/iwyu-qt.imp"

if [ $# -gt 0 ]; then
    files="$*"
else
    # The application's own .cpp files: no tests, no libs/, no generated autogen code.
    files=$(python3 - "$build/compile_commands.json" "$root" <<'PY'
import json, sys
cc = json.load(open(sys.argv[1])); root = sys.argv[2].rstrip('/') + '/'
seen = set()
for e in cc:
    f = e['file']
    if not f.startswith(root): continue
    rel = f[len(root):]
    if rel.startswith(('libs/', 'tests/', 'build')) or '_autogen' in rel or '_deps' in rel: continue
    if '/STC_editor.dir/' not in e.get('output', '') and 'STC_editor.dir' not in e.get('output', ''): continue
    seen.add(f)
print(' '.join(sorted(seen)))
PY
)
fi
[ -n "$files" ] || die "no files to process"
echo "iwyu-fix.sh: $(echo $files | wc -w) file(s), $jobs job(s)" >&2

tmp=$(mktemp -d "${TMPDIR:-/tmp}/stc-iwyu-fix.XXXXXX") || die "mktemp failed"
trap 'rm -rf "$tmp"' EXIT

# 1. IWYU. (Its exit status only says "there were suggestions".)
# shellcheck disable=SC2086
"$iwyu_tool" -p "$build" -j "$jobs" $files -- $maps > "$tmp/raw.txt" 2> "$tmp/raw.err"

# 2. Drop the ignored suggestions.
awk -v ignore="$dir/iwyu-ignore.txt" -f "$dir/iwyu-filter.awk" "$tmp/raw.txt" > "$tmp/filtered.txt"
case $? in 0|10) ;; *) die "awk filter failed" ;; esac

# 3. Apply.
# shellcheck disable=SC2086
"$fix" $dry --noreorder --nocomments --nosafe_headers --basedir "$root" < "$tmp/filtered.txt"
status=$?
if [ -n "$dry" ]; then
    echo "iwyu-fix.sh: dry run - nothing was changed" >&2
else
    echo "iwyu-fix.sh: done; review with 'git diff', rebuild, run the tests, then run it once more" >&2
fi
exit 0
