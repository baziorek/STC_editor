#pragma once
#include <QFrame>
#include "types/stcTags.h"

namespace Ui {
class StcTagsButtons;
}

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

signals:
    void buttonPressed(StcTags tag);

private:
    Ui::StcTagsButtons *ui;
};
