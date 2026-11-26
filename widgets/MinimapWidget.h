#pragma once

#include <QFont>
#include <QPlainTextEdit>
#include <QPointer>

class CodeEditor;
class QTimer;
class QWheelEvent;
class QMouseEvent;

/**
 * @class MinimapWidget
 * @brief A miniature of the document on the side of the editor, like the minimap of VS Code.
 *
 * It shows the whole text of the bound CodeEditor in a tiny font. Scrolling the editor moves
 * the minimap proportionally; clicking or dragging on the minimap scrolls the editor so the
 * clicked place lands in the middle of the viewport, and the wheel over it scrolls the editor.
 * The text is refreshed a moment after the last change (it is expensive on long documents).
 */
class MinimapWidget : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit MinimapWidget(QWidget* parent = nullptr);

    /// Binds the minimap to the document of the given editor (pass nullptr to detach).
    void setEditor(CodeEditor* editor);
    /// The font of the editor the minimap mirrors; the minimap font is a small fraction of it.
    void setBaseFont(const QFont& font);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void scheduleContentUpdate();
    void updateContent();
    /// Moves the minimap scrollbar so it shows the same part of the document as the editor.
    void syncScrollFromEditor();
    /// Scrolls the editor so the viewport fraction `y / height` of the minimap is in the middle.
    void scrollEditorToViewportY(int y);

    QPointer<CodeEditor> editor_;
    QTimer* contentTimer_ = nullptr;
    QFont baseFont_;
    bool syncingScroll_ = false;
    bool dragging_ = false;
};
