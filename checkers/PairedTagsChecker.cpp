#include <algorithm>
#include <vector>
#include <regex>
#include <format>
#include "PairedTagsChecker.h"
using namespace std;

namespace
{
const std::regex tagRegex{R"(\[/?([[:alpha:]][[:alnum:]]*)([^\]]*?)\])"};
} // namespace

struct Tag
{
    std::string tagShortname;
    std::string tagFull;
    std::vector<std::string> attributes;
    unsigned line{}, startingPositionInLine{};

    std::string toString() const
    {
        return format("{}.{}: Tag({}: {}: {})", line, startingPositionInLine, tagShortname, tagFull, attributes2String());
    }
    std::string attributes2String() const
    {
        std::string attributesAsString;
        for (const auto& attribute : attributes)
        {
            attributesAsString += ">" + attribute + "< ";
        }
        return attributesAsString;
    }

    bool isOpening() const
    {
        return ! isClosing();
    }
    bool isClosing() const
    {
        return tagFull[1] == '/'; // TODO: consider using regex
    }
};

std::vector<std::string_view> splitLines(const std::string& text)
{
    std::vector<std::string_view> lines;
    std::string::size_type start = 0;
    std::string::size_type end;

    while ((end = text.find('\n', start)) != std::string::npos)
    {
        lines.emplace_back(text.data() + start, end - start);
        start = end + 1;
    }
    lines.emplace_back(text.data() + start, text.size() - start);

    return lines;
}

std::vector<Tag> extractTags(const std::vector<std::string_view>& lines)
{
    std::vector<Tag> tags;

    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
    {
        std::string_view line = lines[lineNumber];
        auto searchStart = line.cbegin();

        while (searchStart != line.cend())
        {
            std::cmatch match;
            if (std::regex_search(searchStart, line.cend(), match, tagRegex))
            {
                Tag tag;
                tag.tagFull = match[0];
                tag.tagShortname = match[1];

                std::string attributesStr = match[2];
                std::regex attrRegex{R"(\s*([[:alpha:]]+)=['"]?([^'"]+)['"]?)"};
                std::smatch attrMatch;

                auto attrSearchStart = attributesStr.cbegin();
                while (std::regex_search(attrSearchStart, attributesStr.cend(), attrMatch, attrRegex))
                {
                    tag.attributes.emplace_back(attrMatch[1].str() + "=" + attrMatch[2].str());
                    attrSearchStart = attrMatch.suffix().first;
                }

                tag.line = lineNumber + 1;
                searchStart = match.suffix().first;
                tag.startingPositionInLine = searchStart - line.cbegin() - tag.tagFull.size();
                tags.emplace_back(std::move(tag));
            }
            else
            {
                break;
            }
        }
    }

    return tags;
}

std::vector<PairedTagsChecker::TagError> checkIfAllTagsAreClosed(const std::vector<Tag>& tags)
{
    std::vector<PairedTagsChecker::TagError> errorPerLine;
    errorPerLine.reserve(tags.size());

    std::stack<Tag> openTags;
    bool insideRun = false, insideCpp = false, insideCode = false, insidePython = false;

    for (const auto& tag : tags)
    {
        if (insideCpp && tag.tagShortname != "cpp")
        {
            continue; // warning? tags inside cpp will not work, but it can be array indexing
        }
        else if (insideCode && tag.tagShortname != "code")
        {
            // cout << "! Tags inside [code] will not work! Current tag: " << tag.toString() << endl;
            continue;
        }
        else if (insidePython && tag.tagShortname != "py")
        {
            continue; // warning? tags inside python will not work, but it can be array indexing
        }

        if (tag.isOpening())
        {
            if (tag.tagShortname == "img"
                || tag.tagShortname == "a")
            {
                continue; // those tags don't have to be closed
            }
            openTags.emplace(tag);
        }
        else // isClosingTag
        {
            if (openTags.empty())
            {
                auto errorText = std::format("{} is not opened! No tags are opened! You are trying to close the tag in line {} in position {}!", tag.tagShortname, tag.line, tag.startingPositionInLine);
                errorPerLine.emplace_back(tag.line, tag.startingPositionInLine, errorText);
                continue;
            }
            if (openTags.top().tagShortname == tag.tagShortname)
            {
                openTags.pop();
            }
            else
            {
                auto errorText = std::format("{} is not closing in line {} in position {} is not closing! Maybe you want to close {}",
                                             tag.tagShortname, tag.line, tag.startingPositionInLine, openTags.top().tagShortname);
                errorPerLine.emplace_back(tag.line, tag.startingPositionInLine, errorText);
            }
        }

        if (tag.tagShortname == "code")
        {
            insideCode = !insideCode;
        }
        else if (tag.tagShortname == "cpp")
        {
            insideCpp = !insideCpp;
        }
        else if (tag.tagShortname == "py")
        {
            insidePython = !insidePython;
        }
    }

    while (!openTags.empty())
    {
        const auto& tag = openTags.top();
        auto errorText = std::format("{} starting in line {} in position {} is not closing!", tag.tagShortname, tag.line, tag.startingPositionInLine);
        errorPerLine.emplace_back(tag.line, tag.startingPositionInLine, errorText);
        openTags.pop();
    }
    std::sort(begin(errorPerLine), end(errorPerLine));
    return errorPerLine;
}

std::vector<PairedTagsChecker::TagError> PairedTagsChecker::checkTags(const std::string& text)
{
    const auto lines = splitLines(text);
    const auto tags = extractTags(lines);

    return checkIfAllTagsAreClosed(tags);
}

/*
 * TODO: checks:
 * 1. czy sa cudzyslowia
 * 2. czy atrybuty sa niepuste
 * 3. czy jak mamy run to jestesmy w ramach "ext"
 * 4. jak img nie ma alt to sugestia aby zawieral ze wzgledu na osoby niewidzace
 */
