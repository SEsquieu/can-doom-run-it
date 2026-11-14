// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "control_bus.h"

namespace cdr
{
// A single bounded, nonblocking send to IPv4 loopback. No retry queue or waits.
class LoopbackUdpOutput
{
public:
    explicit LoopbackUdpOutput(std::uint16_t port = DefaultOutputPort);
    ~LoopbackUdpOutput();
    LoopbackUdpOutput(const LoopbackUdpOutput&) = delete;
    LoopbackUdpOutput& operator=(const LoopbackUdpOutput&) = delete;

    bool Send(std::string_view event) const noexcept;
    bool IsReady() const { return ready_; }

private:
    std::uintptr_t socket_ = 0;
    std::uint16_t port_;
    bool ready_ = false;
#ifdef _WIN32
    bool winsockStarted_ = false;
#endif
};
}
