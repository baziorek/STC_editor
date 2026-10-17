#pragma once

#include <optional>
#include <QString>
#include <QUrl>

namespace stc::links
{
/// A web page which is previewed when the mouse stays over the link leading to it.
struct PreviewTarget
{
    QUrl url;
    int start = 0;  ///< where the link begins in the line (the `[a href="..."]` tag or the bare address)
    int length = 0;

    bool operator==(const PreviewTarget&) const = default;
};

/// The `http` / `https` address written in `text`; `www.example.pl` gets `https://`.
/// `std::nullopt` for everything else - other schemes (`javascript:`, `file:`, `ftp:`, `mailto:`...), a missing host
/// or a broken address - so that only ordinary web pages are ever opened by the preview.
std::optional<QUrl> normalizedWebAddress(const QString& text);

/// The page to preview when the cursor is at `position` (a place between characters, as QTextCursor gives it)
/// of `line`: the `[a href="..."]` tag, and when `includeBareUrls` is set also an address written in the text
/// (plain text mode). `std::nullopt` when there is no link there or it does not lead to a web page.
std::optional<PreviewTarget> previewTargetAt(const QString& line, int position, bool includeBareUrls);
} // namespace stc::links
