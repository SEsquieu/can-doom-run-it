// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "control_bus.h"

namespace cdr
{
// A single bounded, nonblocking send to IPv4 loopback. No retry queue or waits.
class LoopbackUdpOutput final : public Adapter
{
public:
    explicit LoopbackUdpOutput(std::uint16_t port = DefaultOutputPort, bool subscriptions = false);
    ~LoopbackUdpOutput();
    LoopbackUdpOutput(const LoopbackUdpOutput&) = delete;
    LoopbackUdpOutput& operator=(const LoopbackUdpOutput&) = delete;

    bool Send(std::string_view event) const noexcept;
    const char* Id() const noexcept override { return "udp.loopback"; }
    bool IsReady() const noexcept override { return ready_; }
    bool Send(const Message&, const Definition*) noexcept override;
    void Tick(std::uint64_t tick) noexcept override;
    bool TakeSnapshotRequest() noexcept { bool value=snapshotRequested_; snapshotRequested_=false; return value; }

private:
    std::uintptr_t socket_ = 0;
    std::uint16_t port_;
    bool ready_ = false;
    std::uintptr_t subscriptionSocket_ = 0;
    bool subscriptionReady_ = false, snapshotRequested_ = false;
    std::uint64_t lastRequest_ = 0;
#ifdef _WIN32
    bool winsockStarted_ = false;
#endif
};
}
