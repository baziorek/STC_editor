#pragma once

#include <QDialog>

class QCodeEditor;
class QLabel;

/// A window with the HTML which cpp0x.pl returned for the text, formatted (HtmlPrettyPrinter) and highlighted
/// (QCodeEditor with its XML highlighter - there is no HTML one, but XML tags, attributes and values are colored the same way).
/// It is a snapshot; the "Refresh" button asks for the current HTML.
class HtmlSourceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HtmlSourceDialog(QWidget *parent = nullptr);

    /// Shows the HTML, formatted; the editor stays scrolled where it was (so "Refresh" does not jump to the top)
    void setHtml(const QString &html);

signals:
    void refreshRequested();

private:
    void copyAll();

    QCodeEditor *editor;
    QLabel *statistics;
};
