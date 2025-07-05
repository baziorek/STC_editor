#pragma once

#include <QSet>
#include <QStringList>

QSet<int> calculateModifiedLines(const QStringList& oldLines, const QStringList& newLines);
