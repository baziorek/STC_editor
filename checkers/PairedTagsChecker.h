#pragma once

#include <string>
#include <vector>

namespace PairedTagsChecker
{
std::vector<std::pair<unsigned,std::string>> checkTags(const std::string& text);
};
