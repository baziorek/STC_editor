#include <algorithm>
#include <cctype>
#include <format>
#include <regex>
#include <string_view>
#include <vector>
#include "PairedTagsChecker.h"
#include "StcTagRules.h"
using namespace std;

namespace
{
const std::regex tagRegex{R"(\[/?([[:alpha:]][[:alnum:]_]*)([^\]]*?)\])"};

struct Attribute
{
    std::string name;
    std::string value;
    char quote{};             ///< '"' or '\'' when the value is quoted, 0 otherwise
    bool hasValue{};          ///< there was a '=' after the name
    bool unterminatedQuote{}; ///< the opening quote has no closing one
};

struct Tag
{
    std::string tagShortname;
    std::string tagFull;
    std::vector<Attribute> attributes;
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
            attributesAsString += ">" + attribute.name + (attribute.hasValue ? "=" + attribute.value : "") + "< ";
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
    bool hasAttribute(std::string_view name) const
    {
        return std::ranges::any_of(attributes, [name](const Attribute& attribute) { return attribute.name == name; });
    }
    /// Tag as the user sees it in the text, e.g. "[b]" or "[/b]"
    std::string displayName() const
    {
        return format("[{}{}]", isClosing() ? "/" : "", tagShortname);
    }
};

bool isSpace(char character)
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

void skipSpaces(std::string_view text, std::size_t& position)
{
    while (position < text.size() && isSpace(text[position]))
    {
        ++position;
    }
}

/// Reads the value after '=': in quotes (double or single) or a bare word until the next space
void readAttributeValue(std::string_view text, std::size_t& position, Attribute& attribute)
{
    if (position < text.size() && (text[position] == '"' || text[position] == '\''))
    {
        attribute.quote = text[position];
        const auto closingQuote = text.find(attribute.quote, position + 1);
        if (closingQuote == std::string_view::npos)
        {
            attribute.unterminatedQuote = true;
            attribute.value = text.substr(position + 1);
            position = text.size();
        }
        else
        {
            attribute.value = text.substr(position + 1, closingQuote - position - 1);
            position = closingQuote + 1;
        }
        return;
    }

    const auto valueStart = position;
    while (position < text.size() && !isSpace(text[position]))
    {
        ++position;
    }
    attribute.value = text.substr(valueStart, position - valueStart);
}

/// Splits the text after the tag name (e.g. ` href="x" name="y" ext`) into attributes - flags, quoted and bare values
std::vector<Attribute> parseAttributes(std::string_view text)
{
    std::vector<Attribute> attributes;
    std::size_t position = 0;

    while (true)
    {
        skipSpaces(text, position);
        if (position >= text.size())
        {
            break;
        }

        Attribute attribute;
        const auto nameStart = position;
        while (position < text.size() && !isSpace(text[position]) && text[position] != '=')
        {
            ++position;
        }
        attribute.name = text.substr(nameStart, position - nameStart);

        skipSpaces(text, position);
        if (position < text.size() && text[position] == '=')
        {
            ++position;
            skipSpaces(text, position);
            attribute.hasValue = true;
            readAttributeValue(text, position, attribute);
        }
        attributes.emplace_back(std::move(attribute));
    }

    return attributes;
}

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

    for (std::size_t lineNumber = 0; lineNumber < lines.size(); ++lineNumber)
    {
        std::string lineStr{lines[lineNumber]};
        auto searchStart = lineStr.cbegin();

        while (searchStart != lineStr.cend())
        {
            std::smatch match;
            if (std::regex_search(searchStart, lineStr.cend(), match, tagRegex))
            {
                Tag tag;
                tag.tagFull = match[0];
                tag.tagShortname = match[1];
                tag.attributes = parseAttributes(match[2].str());
                tag.line = lineNumber + 1;
                tag.startingPositionInLine = std::distance(lineStr.cbegin(), match[0].first);
                tags.emplace_back(std::move(tag));

                searchStart = match.suffix().first;
            }
            else
            {
                break;
            }
        }
    }

    return tags;
}

bool isNumber(std::string_view text)
{
    return !text.empty() && std::ranges::all_of(text, [](char character) { return character >= '0' && character <= '9'; });
}

/// Walks through the tags of the document keeping track of open tags, and reports everything that is wrong with them
class TagsAnalyzer
{
public:
    std::vector<PairedTagsChecker::TagError> analyze(const std::vector<Tag>& tags)
    {
        for (const auto& tag : tags)
        {
            processTag(tag);
        }
        reportUnclosedTags();

        std::sort(begin(errors), end(errors));
        return std::move(errors);
    }

private:
    void processTag(const Tag& tag)
    {
        if (isHiddenByCodeBlock(tag))
        {
            return;
        }

        const auto* rule = StcTagRules::findRule(tag.tagShortname);
        if (rule == nullptr)
        {
            addError(tag, format("{} is not an STC tag (unknown tag).", tag.displayName()));
            return;
        }

        if (tag.isOpening())
        {
            processOpeningTag(tag, *rule);
        }
        else if (!processClosingTag(tag, *rule))
        {
            return;
        }
        toggleCodeBlockState(tag);
    }

    /// Tags inside [cpp], [code] and [py] are not interpreted (it can be e.g. array indexing)
    bool isHiddenByCodeBlock(const Tag& tag) const
    {
        return (insideCpp && tag.tagShortname != "cpp")
               || (insideCode && tag.tagShortname != "code")
               || (insidePython && tag.tagShortname != "py");
    }

    void processOpeningTag(const Tag& tag, const StcTagRules::TagRule& rule)
    {
        checkAttributes(tag, rule);

        if (rule.closing == StcTagRules::Closing::OneSided)
        {
            return; // those tags don't have to be closed
        }
        if (tag.tagShortname == "run")
        {
            checkRunPlacement(tag);
        }
        openTags.push_back(tag);
    }

    /// @return false when nothing more should be done with the tag (it closed nothing)
    bool processClosingTag(const Tag& tag, const StcTagRules::TagRule& rule)
    {
        if (rule.closing == StcTagRules::Closing::OneSided)
        {
            addError(tag, format("{} is a one-sided tag, it must not be closed.", tag.displayName()));
            return false;
        }
        if (openTags.empty())
        {
            auto errorText = std::format("{} is not opened! No tags are opened! You are trying to close the tag in line {} in position {}!", tag.tagShortname, tag.line, tag.startingPositionInLine);
            addError(tag, std::move(errorText));
            return false;
        }

        const auto& innermostOpen = openTags.back();
        if (innermostOpen.tagShortname == tag.tagShortname)
        {
            openTags.pop_back();
        }
        else if (isOpen(tag.tagShortname))
        {
            auto errorText = std::format("{} in line {} in position {} does not close the innermost open tag. Maybe you want to close {}",
                                         tag.tagShortname, tag.line, tag.startingPositionInLine, innermostOpen.tagShortname);
            addError(tag, std::move(errorText));
        }
        else
        {
            auto errorText = std::format("{} in line {} in position {} closes a tag that was never opened. The innermost open tag is {}",
                                         tag.tagShortname, tag.line, tag.startingPositionInLine, innermostOpen.tagShortname);
            addError(tag, std::move(errorText));
        }
        return true;
    }

    void toggleCodeBlockState(const Tag& tag)
    {
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

    bool isOpen(std::string_view tagName) const
    {
        return std::ranges::any_of(openTags, [tagName](const Tag& open) { return open.tagShortname == tagName; });
    }

    /// [run] works only in the extended mode of [pkt] or [csv]: [pkt ext] / [csv ext]
    void checkRunPlacement(const Tag& runTag)
    {
        const auto container = std::find_if(openTags.rbegin(), openTags.rend(), [](const Tag& open)
        {
            return open.tagShortname == "pkt" || open.tagShortname == "csv";
        });

        if (container == openTags.rend())
        {
            addError(runTag, "[run] is outside of [pkt] and [csv]. It works only inside [pkt ext] or [csv ext].");
        }
        else if (!container->hasAttribute("ext") && !container->hasAttribute("extended"))
        {
            addError(runTag, format("[run] inside [{0}] without the ext attribute will not work. Use [{0} ext].", container->tagShortname));
        }
    }

    void checkAttributes(const Tag& tag, const StcTagRules::TagRule& rule)
    {
        for (const auto& attribute : tag.attributes)
        {
            checkAttribute(tag, attribute, rule);
        }
        checkRequiredAttributes(tag, rule);
        checkDuplicatedAttributes(tag);
    }

    void checkAttribute(const Tag& tag, const Attribute& attribute, const StcTagRules::TagRule& rule)
    {
        const auto attributeRule = std::ranges::find(rule.attributes, attribute.name, &StcTagRules::AttributeRule::name);
        if (attributeRule == rule.attributes.end())
        {
            addError(tag, format("Tag {} has an unknown attribute \"{}\". {}", tag.displayName(), attribute.name, describeKnownAttributes(rule)));
            return;
        }

        using enum StcTagRules::ValueKind;
        switch (attributeRule->kind)
        {
        case Flag:
            if (attribute.hasValue)
            {
                addError(tag, format("Attribute {} in tag {} is a flag, it takes no value.", attribute.name, tag.displayName()));
            }
            break;
        case NumberOrQuoted:
            if (attribute.hasValue && attribute.quote == 0 && isNumber(attribute.value))
            {
                break;
            }
            [[fallthrough]];
        case QuotedText:
            checkQuotedValue(tag, attribute, *attributeRule);
            break;
        }
    }

    void checkQuotedValue(const Tag& tag, const Attribute& attribute, const StcTagRules::AttributeRule& rule)
    {
        if (!attribute.hasValue || (attribute.quote == 0 && attribute.value.empty()))
        {
            addError(tag, format("Attribute {0} in tag {1} needs a value in double quotes: {0}=\"...\".", attribute.name, tag.displayName()));
        }
        else if (attribute.unterminatedQuote)
        {
            addError(tag, format("Attribute {} in tag {} has no closing quotation mark.", attribute.name, tag.displayName()));
        }
        else if (attribute.quote != '"')
        {
            addError(tag, format("Value of attribute {0} in tag {1} must be in double quotes: {0}=\"...\".", attribute.name, tag.displayName()));
        }
        else if (attribute.value.empty() && !rule.mayBeEmpty)
        {
            addError(tag, format("Attribute {} in tag {} is empty.", attribute.name, tag.displayName()));
        }
    }

    void checkRequiredAttributes(const Tag& tag, const StcTagRules::TagRule& rule)
    {
        for (const auto& attributeRule : rule.attributes)
        {
            if (attributeRule.required && !tag.hasAttribute(attributeRule.name))
            {
                addError(tag, format("Tag {} needs the attribute {}.", tag.displayName(), attributeRule.name));
            }
        }
    }

    void checkDuplicatedAttributes(const Tag& tag)
    {
        for (std::size_t index = 1; index < tag.attributes.size(); ++index)
        {
            const auto& name = tag.attributes[index].name;
            const auto previousEnd = tag.attributes.begin() + index;
            if (std::ranges::any_of(tag.attributes.begin(), previousEnd, [&name](const Attribute& previous) { return previous.name == name; }))
            {
                addError(tag, format("Attribute {} is given twice in tag {}.", name, tag.displayName()));
            }
        }
    }

    static std::string describeKnownAttributes(const StcTagRules::TagRule& rule)
    {
        if (rule.attributes.empty())
        {
            return "This tag takes no attributes.";
        }

        std::string known;
        for (const auto& attributeRule : rule.attributes)
        {
            known += (known.empty() ? "" : ", ");
            known += attributeRule.name;
        }
        return "Known attributes: " + known + ".";
    }

    void reportUnclosedTags()
    {
        while (!openTags.empty())
        {
            const auto& tag = openTags.back();
            auto errorText = std::format("{} starting in line {} in position {} is not closing!", tag.tagShortname, tag.line, tag.startingPositionInLine);
            addError(tag, std::move(errorText));
            openTags.pop_back();
        }
    }

    void addError(const Tag& tag, std::string errorText)
    {
        errors.emplace_back(tag.line, tag.startingPositionInLine, std::move(errorText));
    }

    std::vector<PairedTagsChecker::TagError> errors;
    std::vector<Tag> openTags; // the innermost tag is the last one
    bool insideCpp{}, insideCode{}, insidePython{};
};
} // namespace

std::vector<PairedTagsChecker::TagError> PairedTagsChecker::checkTags(const std::string& text)
{
    const auto lines = splitLines(text);
    const auto tags = extractTags(lines);

    return TagsAnalyzer{}.analyze(tags);
}

/*
 * TODO: checks:
 * 1. jak img nie ma alt to sugestia aby zawieral ze wzgledu na osoby niewidzace
 */
