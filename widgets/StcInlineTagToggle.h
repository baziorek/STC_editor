#pragma once

#include <QString>
#include "types/stcTags.h"

class QPlainTextEdit;

/// Surrounds the selected text of `editor` with `[tag]...[/tag]`, or removes the tags when the selection
/// is already inside of them (or contains them). Without a selection an empty pair is inserted and the
/// cursor is put between the tags.
void stcToggleInlineTag(QPlainTextEdit* editor, const QString& tag);

/// What a button of the tags (StcTagsButtons) does in the editor of a table cell - the same as in the
/// main window: paired tags are toggled around the selection, `[div class="tip"]`, `[pkt ext]`... get
/// their attributes, the link and the image are recognized in the selected text.
/// `[csv]` and `[run]` are not supported (see StcTagsButtons::useInTableCell()).
void stcApplyTag(QPlainTextEdit* editor, StcTags tag);
