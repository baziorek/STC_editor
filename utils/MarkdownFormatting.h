#pragma once

class QPlainTextEdit;
class QString;

/// Markdown formatting helpers for the editor: they work on the current selection (or the current
/// line when nothing is selected) and are meant for SyntaxMode::Markdown documents.
namespace markdownedit
{

/// Wraps the selection in `marker` (`**`, `*`, `~~`, `` ` ``), or removes the marker when the
/// selection is already wrapped in it (or sits between a pair of markers). With no selection an
/// empty pair is inserted and the cursor is put between the markers.
void toggleInline(QPlainTextEdit* editor, const QString& marker);

/// Makes every line touched by the selection a heading of `level` (`#` * level), or removes the
/// heading mark when the line already is a heading of that level.
void toggleHeading(QPlainTextEdit* editor, int level);

/// Adds a `> ` quote marker to every line touched by the selection, or removes one level of
/// quoting when the lines already start with it.
void toggleQuote(QPlainTextEdit* editor);

/// Wraps the selection into `[text](url)`. When the selection contains a URL the text around it
/// becomes the link text. With no selection an empty `[]()` is inserted.
void insertLink(QPlainTextEdit* editor);

/// Inserts `![alt](src)`. A selection which looks like an image path/URL becomes the source with
/// an empty alt text, anything else becomes the alt text.
void insertImage(QPlainTextEdit* editor);

/// Wraps the selection into a fenced code block with `language` in the info string
/// (```language ... ```). With no selection an empty block is inserted.
void insertCodeBlock(QPlainTextEdit* editor, const QString& language);

} // namespace markdownedit
