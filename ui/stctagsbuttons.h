#pragma once
#include <QFrame>

namespace Ui {
class StcTagsButtons;
}

enum class StdTags: std::uint8_t
{
    RUN,
    CPP,
    PY,
    CODE,
    DIV,
    DIV_WARNING,
    DIV_TIP,
    A_HREF,
    PKT,
    CSV,
    BOLD,
    QUOTE,
    H1,
    H2,
    H3,
    H4
};

class StcTagsButtons : public QFrame
{
    Q_OBJECT

public:
    explicit StcTagsButtons(QWidget *parent = nullptr);
    ~StcTagsButtons();

signals:
    void buttonPressed(StdTags tag);

private:
    Ui::StcTagsButtons *ui;
};
