# Post-processes include-what-you-use's report (see cmake/iwyu-filter.sh).
#
#   awk -v ignore=<patterns file> -f iwyu-filter.awk <iwyu output>
#
# Removes suggested "#include" lines that match a pattern from the ignore file - both from the
# "should add" list and from the "full include-list" - and drops the whole report of a file that
# has nothing left to do ("has correct #includes" files and files whose only suggestions were
# ignored). Everything outside the per-file reports (compiler warnings, ...) is passed through.
#
# Exit status: 10 if anything is left in the output, 0 if the output is empty of real content.

BEGIN {
    npat = 0
    while ((getline line < ignore) > 0) {
        if (line ~ /^[[:space:]]*(#|$)/) continue
        pat[npat++] = line
    }
    close(ignore)
    state = "out"; nbuf = 0; kept_add = 0; kept_rm = 0; other = 0; actionable = 0
}

function noisy(l,    i) {
    for (i = 0; i < npat; i++) if (l ~ pat[i]) return 1
    return 0
}

function flush(    i) {
    if (nbuf > 0 && (kept_add > 0 || kept_rm > 0)) {
        for (i = 0; i < nbuf; i++) print buf[i]
        actionable = 1
    }
    nbuf = 0; kept_add = 0; kept_rm = 0; state = "out"
}

# "(<file> has correct #includes/fwd-decls)" - nothing to do for that file.
/^\(.* has correct #includes\/fwd-decls\)$/ { next }

/ should add these lines:$/ { flush(); state = "add"; buf[nbuf++] = $0; next }
/ should remove these lines:$/ { state = "rm"; buf[nbuf++] = $0; next }
/^The full include-list for / { state = "full"; buf[nbuf++] = $0; next }
/^---$/ { if (state == "full") { buf[nbuf++] = $0; flush(); next } }

state == "add" {
    if ($0 == "") { buf[nbuf++] = $0; next }
    if ($0 ~ /^#include/ && noisy($0)) next
    buf[nbuf++] = $0; kept_add++; next
}
state == "rm" {
    if ($0 == "") { buf[nbuf++] = $0; next }
    buf[nbuf++] = $0; kept_rm++; next
}
state == "full" {
    if ($0 ~ /^#include/ && noisy($0)) next
    buf[nbuf++] = $0; next
}

# Outside of any report.
{
    print
    if ($0 !~ /^[[:space:]]*$/) other++
}

END {
    flush()
    exit ((actionable || other) ? 10 : 0)
}
