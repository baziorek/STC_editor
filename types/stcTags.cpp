#include "types/stcTags.h"
using std::make_pair;

using enum StdTags;

std::map<StdTags, QString> tagsClasses =
{
    make_pair(RUN, "run"),
    make_pair(CPP, "cpp"),
    make_pair(PY, "py"),
    make_pair(CODE, "code"),
    make_pair(DIV, "div"),
    make_pair(DIV_WARNING, "div_warning"),
    make_pair(DIV_TIP, "div_tip"),
    make_pair(A_HREF, "a_href"),
    make_pair(PKT, "pkt"),
    make_pair(CSV, "csv"),
    make_pair(BOLD, "b"),
    make_pair(ITALIC, "i"),
    make_pair(UNDERLINED, "u"),
    make_pair(STRUCK_OUT, "s"),
    make_pair(QUOTE, "cytat"),
    make_pair(H1, "h1"),
    make_pair(H2, "h2"),
    make_pair(H3, "h3"),
    make_pair(H4, "h4"),
};
