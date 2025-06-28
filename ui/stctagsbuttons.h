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

signals:
    void buttonPressed(StcTags tag);

private:
    Ui::StcTagsButtons *ui;
};
