// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class player_t;

namespace cdr
{
// Called only on a Use rising edge, alongside normal engine use processing.
void PublishPlayerUse(const player_t* player);
}
