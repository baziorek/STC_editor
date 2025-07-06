#pragma once

#include <string>
#include <vector>

namespace PairedTagsChecker
{
struct TagError
{
    int line;
    int positionInLine;
    std::string errorText;
    bool operator<(const TagError& tagError) const
    {
        return std::make_tuple(line, positionInLine, errorText) < std::make_tuple(tagError.line, tagError.positionInLine, tagError.errorText);
    }
};

std::vector<TagError> checkTags(const std::string& text);
};
