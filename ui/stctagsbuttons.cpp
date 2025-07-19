#include "stctagsbuttons.h"
#include "ui_stctagsbuttons.h"


StcTagsButtons::StcTagsButtons(QWidget *parent)
    : QFrame(parent)
    , ui(new Ui::StcTagsButtons)
{
    ui->setupUi(this);

    connect(ui->button_run, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::RUN);
    });
    connect(ui->button_cpp, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::CPP);
    });
    connect(ui->button_py, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::PY);
    });
    connect(ui->button_code, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::CODE);
    });
    connect(ui->button_div_tip, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::DIV_TIP);
    });
    connect(ui->button_div_warning, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::DIV_WARNING);
    });
    connect(ui->button_div, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::DIV);
    });
    connect(ui->button_cytat, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::QUOTE);
    });
    connect(ui->button_href, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::A_HREF);
    });
    connect(ui->button_pkt, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::PKT);
    });
    connect(ui->button_csv, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::CSV);
    });
    connect(ui->button_bold, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::BOLD);
    });
    connect(ui->button_italic, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::ITALIC);
    });
    connect(ui->button_stroke, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::STRUCK_OUT);
    });
    connect(ui->button_underlined, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::UNDERLINED);
    });
    connect(ui->button_h1, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::H1);
    });
    connect(ui->button_h2, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::H2);
    });
    connect(ui->button_h3, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::H3);
    });
    connect(ui->button_h4, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::H4);
    });
    connect(ui->button_subsctipt, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::SUBSCRIPT);
    });
    connect(ui->button_supsctipt, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::SUPSCRIPT);
    });
    connect(ui->button_teleType, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::TELE_TYPE);
    });
    connect(ui->button_img, &QPushButton::pressed, [this] {
        emit buttonPressed(StcTags::IMG);
    });
}

StcTagsButtons::~StcTagsButtons()
{
    delete ui;
}

QMultiMap<QString, QKeySequence> StcTagsButtons::listOfShortcuts() const
{
    QMultiMap<QString, QKeySequence> result;

    for (QAbstractButton* btn : findChildren<QAbstractButton*>()) {
        QString text = btn->text();              // e.g. "&H1" or "Insert &Image"
        int amp = text.indexOf('&');

        if (amp >= 0 && amp + 1 < text.length()) {
            QChar mnemonicChar = text[amp + 1].toUpper(); // e.g. 'H'
            QKeySequence shortcut(Qt::ALT | mnemonicChar.unicode());

            // Removing '&' from text, e.g. "&H1" -> "H1"
            QString visibleText = text;
            visibleText.remove('&');

            auto description = QString("Presses button %1").arg(visibleText);

            result.insert(description, shortcut);
        }
    }

    return result;
}
