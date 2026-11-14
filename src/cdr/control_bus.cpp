// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_bus.h"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <utility>

namespace cdr
{
Bus::Bus(Sink sink) : sink_(std::move(sink)) {}

bool Bus::Trigger(Control control, std::uint64_t tick, std::uint32_t player)
{
    // A closed identifier set keeps arbitrary mod strings out of serialization.
    if (control != Control::PlayerUse) return false;

    const auto sequence = ++counters_.emitted;
    std::array<char, MaxDatagramSize> bytes{};
    const int size = std::snprintf(bytes.data(), bytes.size(),
        "{\"schema\":1,\"sequence\":%" PRIu64 ",\"type\":\"trigger\","
        "\"control\":\"player.use\",\"tick\":%" PRIu64 ",\"player\":%" PRIu32 "}",
        sequence, tick, player);
    if (size > 0 && static_cast<std::size_t>(size) < bytes.size() && sink_ &&
        sink_(std::string_view(bytes.data(), static_cast<std::size_t>(size))))
    {
        ++counters_.submitted;
        return true;
    }
    ++counters_.dropped;
    return false;
}
}
