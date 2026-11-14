// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>

namespace cdr
{
constexpr std::size_t MaxDatagramSize = 256;
constexpr std::uint16_t DefaultOutputPort = 19793;

enum class Control { PlayerUse };

struct Counters
{
    std::uint64_t emitted = 0;
    std::uint64_t submitted = 0;
    std::uint64_t dropped = 0;
};

// Owned by the simulation thread. A sink must return promptly, must not throw,
// and may only borrow the serialized bytes for the duration of the call.
class Bus
{
public:
    using Sink = std::function<bool(std::string_view)>;
    explicit Bus(Sink sink);
    bool Trigger(Control control, std::uint64_t tick, std::uint32_t player);
    const Counters& GetCounters() const { return counters_; }

private:
    Sink sink_;
    Counters counters_;
};
}
