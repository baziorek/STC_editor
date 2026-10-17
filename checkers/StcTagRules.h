#pragma once

#include <span>
#include <string_view>

/**
 * @brief Table of the STC tags known to the checker: whether a tag is closed, which attributes it takes.
 *
 * Source: the STC course on https://cpp0x.pl/kursy/Kurs-STC/169 and the patterns used by the syntax highlighter
 * (stcSyntaxPatterns.cpp). The table is header-only (no Qt), so the checker tests don't need extra sources.
 */
namespace StcTagRules
{
enum class Closing
{
    Paired,   ///< [x]...[/x]
    OneSided, ///< [x ...] - must not be closed
};

enum class ValueKind
{
    Flag,           ///< bare word without a value, e.g. `ext`, `header`, `autofit`
    QuotedText,     ///< value in double quotes, e.g. `href="https://..."`
    NumberOrQuoted, ///< a number (`id=123`) or text in double quotes (`id="123"`), like `id` of [doc]
};

struct AttributeRule
{
    std::string_view name;
    ValueKind kind = ValueKind::QuotedText;
    bool required = false;
    bool mayBeEmpty = false; ///< e.g. `alt=""` is legal, `href=""` is not
};

struct TagRule
{
    std::string_view name;
    Closing closing = Closing::Paired;
    std::span<const AttributeRule> attributes{};
};

/// @return the rule of the tag with that (lower case) name, or nullptr when it is not an STC tag.
inline const TagRule* findRule(std::string_view tagName)
{
    using enum ValueKind;

    static constexpr AttributeRule divAttributes[] = {{.name = "class"}};
    static constexpr AttributeRule pktAttributes[] = {{.name = "ext", .kind = Flag}, {.name = "extended", .kind = Flag}};
    static constexpr AttributeRule csvAttributes[] = {{.name = "ext", .kind = Flag},
                                                      {.name = "extended", .kind = Flag},
                                                      {.name = "header", .kind = Flag}};
    static constexpr AttributeRule codeAttributes[] = {{.name = "src"}};
    static constexpr AttributeRule aAttributes[] = {{.name = "href", .required = true}, {.name = "name"}};
    static constexpr AttributeRule imgAttributes[] = {{.name = "src"},
                                                      {.name = "alt", .mayBeEmpty = true},
                                                      {.name = "opis", .mayBeEmpty = true},
                                                      {.name = "autofit", .kind = Flag}};
    static constexpr AttributeRule docAttributes[] = {{.name = "id", .kind = NumberOrQuoted, .required = true},
                                                      {.name = "name"}};
    static constexpr AttributeRule topicAttributes[] = {{.name = "id", .required = true}};
    static constexpr AttributeRule relAttributes[] = {{.name = "list", .required = true},
                                                      {.name = "header", .kind = Flag}};

    using enum Closing;
    static constexpr TagRule rules[] = {
        // Formatting from HTML (no attributes)
        {"b"}, {"i"}, {"u"}, {"s"}, {"tt"}, {"sub"}, {"sup"},
        {"h1"}, {"h2"}, {"h3"}, {"h4"}, {"h5"}, {"h6"},
        {"cytat"},
        // Frames, lists and tables
        {"div", Paired, divAttributes},
        {"pkt", Paired, pktAttributes},
        {"csv", Paired, csvAttributes},
        {"run"},
        // Code (cpp and py accept `src` too, the highlighter does)
        {"code", Paired, codeAttributes},
        {"cpp", Paired, codeAttributes},
        {"py", Paired, codeAttributes},
        {"log"},
        // One-sided tags
        {"a", OneSided, aAttributes},
        {"img", OneSided, imgAttributes},
        {"doc", OneSided, docAttributes},
        {"tut", OneSided, docAttributes},
        {"art", OneSided, docAttributes},
        {"topic", OneSided, topicAttributes},
        {"t", OneSided, topicAttributes}, // alias of [topic]
        {"doc_rel", OneSided, relAttributes},
        {"tut_rel", OneSided, relAttributes},
        {"art_rel", OneSided, relAttributes},
        // Google search
        {"google"},
        {"g"}, // alias of [google]
    };

    for (const auto& rule : rules)
    {
        if (rule.name == tagName)
        {
            return &rule;
        }
    }
    return nullptr;
}
} // namespace StcTagRules
