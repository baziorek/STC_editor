#pragma once
#include <cstdint>     // std::uint8_t
#include <type_traits> // std::underlying_type_t
#include <map>
#include <QString>

enum class StcTags: std::uint32_t
{
    NONE        = 0x00,
    RUN         = 0x01,
    CPP         = 0x02,
    PY          = 0x04,
    CODE        = 0x08,
    DIV         = 0x10,
    DIV_WARNING = 0x20,
    DIV_TIP     = 0x40,
    A_HREF      = 0x80,
    PKT         = 0x100,
    CSV         = 0x200,
    BOLD        = 0x400,
    ITALIC      = 0x800,
    UNDERLINED  = 0x1000,
    STRUCK_OUT  = 0x2000,
    H1          = 0x4000,
    H2          = 0x8000,
    H3          = 0x10000,
    H4          = 0x20000,
    QUOTE       = 0x40000,
    SUBSCRIPT   = 0x80000,
    SUPSCRIPT   = 0x100000,
    TELE_TYPE   = 0x200000,
    IMG         = 0x400000,
};
using StdTagsUnderlying = std::underlying_type_t<StcTags>;

inline bool isFlagEnabled(StdTagsUnderlying tags, StcTags flag)
{
    return std::to_underlying(flag) & tags;
}

extern std::map<StcTags, QString> tagsClasses;
