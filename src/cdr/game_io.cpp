// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_io.h"
#include "control_bus.h"
#include "udp_output.h"

#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "g_levellocals.h"
#include "playsim/actor.h"
#include "playsim/d_player.h"
#include "printf.h"

#include <cinttypes>

CVAR(Bool, cdr_io_enabled, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

namespace
{
struct LocalOutput
{
    cdr::LoopbackUdpOutput output;
    cdr::Bus bus{[this](std::string_view event) { return output.Send(event); }};
};

LocalOutput& GetLocalOutput()
{
    static LocalOutput state;
    return state;
}
}

void cdr::PublishPlayerUse(const player_t* player)
{
    // External effects are local/live only. Network authority and replay
    // behavior must be designed before enabling physical adapters.
    if (!cdr_io_enabled || player != &players[consoleplayer] || demoplayback || netgame ||
        !player->mo || !player->mo->Level) return;
    GetLocalOutput().bus.Trigger(Control::PlayerUse,
        static_cast<std::uint64_t>(player->mo->Level->maptime),
        static_cast<std::uint32_t>(consoleplayer));
}

CCMD(cdr_io_status)
{
    const auto& state = GetLocalOutput();
    const auto& counters = state.bus.GetCounters();
    Printf("Native I/O: enabled=%d ready=%d UDP 127.0.0.1:%u "
        "emitted=%" PRIu64 " submitted=%" PRIu64 " dropped=%" PRIu64 "\n",
        int(bool(cdr_io_enabled)), int(state.output.IsReady()), unsigned(cdr::DefaultOutputPort),
        counters.emitted, counters.submitted, counters.dropped);
}
