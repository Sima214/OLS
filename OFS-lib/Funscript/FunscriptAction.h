#pragma once

#include "OFS_BinarySerialization.h"

#include <cstdint>
#include <limits>

#include "OFS_Util.h"
#include "OFS_VectorSet.h"

struct FunscriptAction {
public:
    enum class ModeFlagBits : uint8_t {
        Step = 0b0000'0001,
    };
    using ModeFlags = Flags<ModeFlagBits>;

public:
    // timestamp as floating point seconds
    // instead of integer milliseconds
    float atS;
    int16_t pos;
    ModeFlags flags;
    uint8_t tag;

    template<typename S>
    void serialize(S& s)
    {
        s.ext(*this, bitsery::ext::Growable{},
            [](S& s, FunscriptAction& o) {
                s.value4b(o.atS);
                s.value2b(o.pos);
                s.object(o.flags);
                s.value1b(o.tag);
            });
    }

    constexpr FunscriptAction() noexcept
    : atS(std::numeric_limits<float>::min()), pos(std::numeric_limits<int16_t>::min()), flags{}, tag(0)
    {
    }

    constexpr FunscriptAction(float at, int32_t pos) noexcept : atS(at),
                                                                pos(pos),
                                                                flags{},
                                                                tag(0)
    {
    }

    constexpr FunscriptAction(float at, int32_t pos, uint8_t tag) noexcept
    : atS(at),
      pos(pos),
      flags{},
      tag(tag)
    {
    }

    constexpr FunscriptAction(float at, int32_t pos, ModeFlags flags) noexcept : atS(at),
                                                                                 pos(pos),
                                                                                 flags(flags),
                                                                                 tag(0)
    {
    }

    constexpr FunscriptAction(float at, int32_t pos, ModeFlags flags, uint8_t tag) noexcept : atS(at),
                                                                                              pos(pos),
                                                                                              flags(flags),
                                                                                              tag(tag)
    {
    }

    constexpr bool operator==(FunscriptAction b) const noexcept
    {
        return this->atS == b.atS && this->pos == b.pos;
    }

    constexpr bool operator!=(FunscriptAction b) const noexcept
    {
        return !(*this == b);
    }

    constexpr bool operator<(FunscriptAction b) const noexcept
    {
        return this->atS < b.atS;
    }
};

struct FunscriptActionHashfunction {
    inline std::size_t operator()(FunscriptAction s) const noexcept
    {
        static_assert(sizeof(FunscriptAction) == sizeof(int64_t));
        return *(int64_t*)&s;
    }
};

struct ActionLess {
    constexpr bool operator()(const FunscriptAction& a, const FunscriptAction& b) const noexcept
    {
        return a.atS < b.atS;
    }
};

static_assert(sizeof(FunscriptAction) == 8);
using FunscriptArray = vector_set<FunscriptAction, ActionLess>;
