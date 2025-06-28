#pragma once
#include <cstdint>     // std::uint8_t
#include <type_traits> // std::underlying_type_t
#include <map>
#include <QString>

enum class StdTags: std::uint32_t
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
    QUOTE       = 0x1000,
    H1          = 0x2000,
    H2          = 0x4000,
    H3          = 0x8000,
    H4          = 0x10000
};
using StdTagsUnderlying = std::underlying_type_t<StdTags>;

inline bool isFlagEnabled(StdTagsUnderlying tags, StdTags flag)
{
    return std::to_underlying(flag) & tags;
}

extern std::map<StdTags, QString> tagsClasses;
