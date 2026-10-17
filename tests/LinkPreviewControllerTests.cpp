#include <gtest/gtest.h>

#include <QApplication>
#include <QByteArray>
#include <QEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include "widgets/LinkPreviewController.h"

/// The controller is an event filter of the editor and it is a child of the editor, so it is still installed when
/// the editor is being destroyed. The events sent in that moment (the editor is hidden and loses the focus) reach
/// the filter when the editor is already only a QWidget: UBSan (vptr) reports any call of an editor's method then.
/// Run with the sanitizers on (the default Debug build) and with UBSAN_OPTIONS=halt_on_error=1 to see it fail.

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen") : qgetenv("QT_QPA_PLATFORM"));
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace
{
LinkPreviewController::BareUrlsAreLinks bareUrlsAreLinks()
{
    return [] { return true; };
}
} // namespace

TEST(LinkPreviewControllerTest, DestroyingAVisibleEditorIsSafe)
{
    auto* editor = new QPlainTextEdit;
    editor->setPlainText(QStringLiteral("[a href=\"https://example.com\"]link[/a]"));
    auto* controller = new LinkPreviewController(editor, bareUrlsAreLinks());
    QPointer<LinkPreviewController> watcher(controller);
    editor->show();
    editor->setFocus();
    QApplication::processEvents();

    delete editor; // the events of the destruction go through the filter

    EXPECT_TRUE(watcher.isNull()) << "the controller is owned by the editor";
}
