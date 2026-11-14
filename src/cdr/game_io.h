// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
class player_t;
namespace cdr
{
void PublishPlayerUse(const player_t* player);
void UpdateRuntime(bool focused,bool live,std::uint64_t tick);
void ShutdownRuntime();
}
