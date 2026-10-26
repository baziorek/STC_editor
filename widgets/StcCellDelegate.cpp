#include <algorithm>
#include <QAbstractItemView>
#include <QApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTableView>
#include <QTextDocument>
#include "StcCellDelegate.h"
#include "utils/STCSyntaxHighlighter.h"

namespace
{
constexpr QLatin1StringView RUN_OPEN{"[run]"};
constexpr QLatin1StringView RUN_CLOSE{"[/run]"};
constexpr const char* FORMATTED_PROPERTY = "stcFormatted";
constexpr int MAX_CACHED_DOCUMENTS = 256;

bool isFormattedIndex(const QModelIndex& index)
{
    return index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
}
}

namespace stc::tablecell
{
bool isWrappedInRun(const QString& text)
{
    const QString t = text.trimmed();
    if (t.size() < RUN_OPEN.size() + RUN_CLOSE.size() || !t.startsWith(RUN_OPEN) || !t.endsWith(RUN_CLOSE))
    {
        return false;
    }
    return t.indexOf(RUN_CLOSE) == t.size() - RUN_CLOSE.size() && t.indexOf(RUN_OPEN, RUN_OPEN.size()) == -1;
}

Decoded decode(const QString& stc)
{
    if (!isWrappedInRun(stc))
    {
        return {stc, false};
    }
    const QString t = stc.trimmed();
    return {t.mid(RUN_OPEN.size(), t.size() - RUN_OPEN.size() - RUN_CLOSE.size()), true};
}

QString encode(const QString& inner, bool formatted)
{
    if (formatted)
    {
        return inner.isEmpty() ? inner : RUN_OPEN + inner + RUN_CLOSE;
    }
    QString result = inner;
    result.replace(QStringLiteral("\r\n"), QStringLiteral(" ")).replace(u'\n', u' ').replace(u'\r', u' ');
    return result;
}

bool hasRunInsideUnwrapped(const QString& inner, bool formatted)
{
    return !formatted && (inner.contains(RUN_OPEN) || inner.contains(RUN_CLOSE));
}

int separatorsInText(const QString& text)
{
    int count = 0;
    bool inQuotes = false;
    bool inTag = false;
    for (const QChar c : text)
    {
        if (c == u'[')
            inTag = true;
        else if (c == u']')
            inTag = false;
        else if (c == u'"')
            inQuotes = !inQuotes;
        else if (c == u';' && !inQuotes && !inTag)
            ++count;
    }
    return count;
}
}

StcCellItem::StcCellItem(const QString& text, bool formatted)
    : QTableWidgetItem(text)
{
    // The check state has to be set, otherwise the check box is not drawn at all.
    setFlags(flags() | Qt::ItemIsUserCheckable);
    setFormatted(formatted);
}

QTableWidgetItem* StcCellItem::clone() const
{
    return new StcCellItem(*this);
}

StcCellDelegate::StcCellDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

StcCellDelegate::~StcCellDelegate() = default;

QWidget* StcCellDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const
{
    const bool formatted = isFormattedIndex(index);

    auto* editor = new QPlainTextEdit(parent);
    editor->setProperty(FORMATTED_PROPERTY, formatted);
    editor->setTabChangesFocus(true);
    if (formatted)
    {
        new STCSyntaxHighlighter(editor->document()); // owned by the document
    }

    // `createEditor` is const, the signal is not
    const_cast<StcCellDelegate*>(this)->cellEditorCreated(editor, formatted);
    return editor;
}

void StcCellDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* edit = qobject_cast<QPlainTextEdit*>(editor);
    if (!edit)
    {
        QStyledItemDelegate::setEditorData(editor, index);
        return;
    }
    edit->setPlainText(index.data(Qt::EditRole).toString());
    edit->moveCursor(QTextCursor::End);
}

void StcCellDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* edit = qobject_cast<QPlainTextEdit*>(editor);
    if (!edit)
    {
        QStyledItemDelegate::setModelData(editor, model, index);
        return;
    }
    QString text = edit->toPlainText();
    if (!edit->property(FORMATTED_PROPERTY).toBool())
    {
        text = stc::tablecell::encode(text, false); // a new line would start a new row
    }
    model->setData(index, text, Qt::EditRole);
}

void StcCellDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex&) const
{
    QRect rect = option.rect;
    if (editor->property(FORMATTED_PROPERTY).toBool())
    {
        // formatted cells may have several lines
        rect.setHeight(std::max(rect.height(), editor->fontMetrics().lineSpacing() * 3 + 12));
    }
    editor->setGeometry(rect);
}

bool StcCellDelegate::eventFilter(QObject* object, QEvent* event)
{
    auto* edit = qobject_cast<QPlainTextEdit*>(object);
    if (edit && event->type() == QEvent::KeyPress)
    {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
        {
            const bool formatted = edit->property(FORMATTED_PROPERTY).toBool();
            if (formatted && (key->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
            {
                edit->insertPlainText(QStringLiteral("\n"));
                return true;
            }
            emit commitData(edit);
            emit closeEditor(edit, QAbstractItemDelegate::NoHint);
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

int StcCellDelegate::textWidthOf(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

    int columnWidth = opt.rect.width();
    if (auto* table = qobject_cast<const QTableView*>(opt.widget); table && index.isValid())
    {
        columnWidth = table->columnWidth(index.column());
    }
    opt.rect = QRect(0, 0, std::max(columnWidth, 20), 100);
    opt.text.clear();
    return std::max(style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget).width(), 10);
}

QTextDocument& StcCellDelegate::documentFor(const QString& text, const QFont& font, int width) const
{
    auto it = m_documents.find(text);
    if (it == m_documents.end())
    {
        if (m_documents.size() >= MAX_CACHED_DOCUMENTS)
        {
            m_documents.clear();
        }
        auto doc = std::make_shared<QTextDocument>();
        doc->setDocumentMargin(0);
        new STCSyntaxHighlighter(doc.get()); // owned by the document
        doc->setDefaultFont(font);
        doc->setPlainText(text); // the highlighter reformats the blocks on this change
        it = m_documents.insert(text, std::move(doc));
    }
    QTextDocument& doc = *it.value();
    if (doc.defaultFont() != font)
    {
        doc.setDefaultFont(font);
    }
    doc.setTextWidth(width);
    return doc;
}

void StcCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if (!isFormattedIndex(index))
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QString text = opt.text;
    opt.text.clear();

    // background, selection and the check box - everything but the text
    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    QTextDocument& doc = documentFor(text, opt.font, textRect.width());

    // centered vertically, the same way as the text of the plain cells
    const int offsetY = std::max(0, (textRect.height() - static_cast<int>(doc.size().height())) / 2);

    painter->save();
    painter->setClipRect(textRect);
    painter->translate(textRect.left(), textRect.top() + offsetY);
    doc.drawContents(painter, QRectF(0, 0, textRect.width(), textRect.height()));
    painter->restore();
}

QSize StcCellDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    if (!isFormattedIndex(index))
    {
        return size;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QTextDocument& doc = documentFor(opt.text, opt.font, textWidthOf(option, index));
    size.setHeight(std::max(size.height(), static_cast<int>(doc.size().height()) + 6));
    return size;
}
