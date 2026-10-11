#include "HeaderNesting.h"

std::vector<int> headerParents(const std::vector<int>& levels)
{
    std::vector<int> parents;
    parents.reserve(levels.size());

    // positions of the headers still open (above the current one), with increasing levels
    std::vector<int> open;
    for (int position = 0; position < static_cast<int>(levels.size()); ++position)
    {
        while (!open.empty() && levels[open.back()] >= levels[position])
            open.pop_back();

        parents.push_back(open.empty() ? -1 : open.back());
        open.push_back(position);
    }
    return parents;
}
