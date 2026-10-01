#pragma once

#include <functional>
#include <QString>
#include <QTextCharFormat>

/// Highlighting of a *fragment* of a line (`[from, to)` of `text`) in Python, XML and JSON.
///
/// The rules are built on top of https://github.com/ArsMasiuk/QCodeEditor (fork of
/// https://github.com/Megaxela/QCodeEditor, MIT): keyword lists come from its `python.xml` and
/// the colors from its `QSyntaxStyle`. QCodeEditor's own highlighters (QPythonHighlighter,
/// QXMLHighlighter, ...) are `QSyntaxHighlighter`s bound to a whole document, so they can't be used
/// for a piece of a line, e.g. the content of `[py]...[/py]` - hence the same idea, but working on fragments.
///
/// Nothing here depends on the STC markup, so the same code highlights an STC code block
/// and an entire file opened as Python/XML/JSON.
namespace stc::codehl
{
/// Called for every highlighted range. `start` is an index in the whole `text`, not in the fragment.
using FormatSetter = std::function<void(int start, int length, const QTextCharFormat& format)>;

/// Constructs which continue on the next line. Every `highlight*` function takes the state
/// from the end of the previous line and returns the state at the end of the fragment.
enum PythonState : int
{
    PY_STATE_NONE          = 0,
    PY_STATE_TRIPLE_DOUBLE = 1, ///< inside `"""..."""` which has not been closed yet
    PY_STATE_TRIPLE_SINGLE = 2, ///< inside `'''...'''` which has not been closed yet
    PY_STATE_MASK          = 3,
};

enum XmlState : int
{
    XML_STATE_NONE    = 0,
    XML_STATE_COMMENT = 1, ///< inside `<!-- ... -->` which has not been closed yet
    XML_STATE_IN_TAG  = 2, ///< inside `<tag ...` - attributes are expected until `>` is found
    XML_STATE_MASK    = 3,
};

int highlightPython(const QString& text, int from, int to, int stateIn, const FormatSetter& setFormat);
int highlightXml(const QString& text, int from, int to, int stateIn, const FormatSetter& setFormat);
/// JSON has no construct spanning lines, so there is no state.
void highlightJson(const QString& text, int from, int to, const FormatSetter& setFormat);
} // namespace stc::codehl
