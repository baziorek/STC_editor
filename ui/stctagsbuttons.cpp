#include "stctagsbuttons.h"
#include "ui_stctagsbuttons.h"


StcTagsButtons::StcTagsButtons(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::StcTagsButtons)
{
    ui->setupUi(this);

    connect(ui->button_run, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::RUN);
    });
    connect(ui->button_cpp, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::CPP);
    });
    connect(ui->button_py, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::PY);
    });
    connect(ui->button_code, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::CODE);
    });
    connect(ui->button_div_tip, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::DIV_TIP);
    });
    connect(ui->button_div_warning, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::DIV_WARNING);
    });
    connect(ui->button_cytat, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::QUOTE);
    });
    connect(ui->button_href, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::A_HREF);
    });
    connect(ui->button_pkt, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::PKT);
    });
    connect(ui->button_csv, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::CSV);
    });
    connect(ui->button_bold, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::BOLD);
    });
    connect(ui->button_italic, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::ITALIC);
    });
    connect(ui->button_h1, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::H1);
    });
    connect(ui->button_h2, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::H2);
    });
    connect(ui->button_h3, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::H3);
    });
    connect(ui->button_h4, &QPushButton::pressed, [this] {
        emit buttonPressed(StdTags::H4);
    });
}

StcTagsButtons::~StcTagsButtons()
{
    delete ui;
}
