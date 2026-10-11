#pragma once

#include <vector>

/// The tree of headers ([h1]...[h6]) of a document: for each header (given by its level, 1 for h1, in document order)
/// the position of its parent, -1 for a top-level one. The parent is the closest header above with a smaller level.
/// A header closes the sections of its own level and of deeper levels.
std::vector<int> headerParents(const std::vector<int>& levels);
