#pragma once
#include <QFrame>
#include <QMap>
#include "types/stcTags.h"

namespace Ui {
class StcTagsButtons;
}
class QAbstractButton;

class StcTagsButtons : public QFrame
{
    Q_OBJECT

public:
    explicit StcTagsButtons(QWidget *parent = nullptr);
    ~StcTagsButtons();

    QMultiMap<QString, QKeySequence> listOfShortcuts() const;

    /// Mode for the table editor: the same buttons as in the main window, except for the ones which make no
    /// sense inside of a table cell (`[csv]` and `[run]`). The buttons do not take the keyboard focus,
    /// so the edited cell keeps it.
    void useInTableCell();

    /// Markdown documents get the same buttons, but only the ones which have a markdown equivalent
    /// stay visible (`[run]`, the `[div]` blocks, `[pkt]`, `[csv]`, sub/superscript, the typewriter
    /// and the underline have none and are hidden). MainWindow then formats with markdown syntax.
    void setMarkdownMode(bool enabled);

signals:
    void buttonPressed(StcTags tag);

private:
    Ui::StcTagsButtons *ui;
    /// The STC texts of the buttons, kept so setMarkdownMode() can put them back.
    QMap<QAbstractButton*, QString> m_originalButtonTexts;
};
