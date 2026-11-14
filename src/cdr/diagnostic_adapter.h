// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "control_bus.h"
namespace cdr
{
struct DiagnosticValue { Value value{}; Phase phase=Phase::None; std::uint64_t changes=0; };
class DiagnosticAdapter final : public Adapter
{
public:
    const char* Id() const noexcept override { return "native.diagnostic"; }
    bool IsReady() const noexcept override { return true; }
    bool Send(const Message& m,const Definition* d) noexcept override
    {
        if ((m.kind==Kind::Event || m.kind==Kind::State || m.kind==Kind::Definition) && m.control<MaxControls)
        {
            auto& v=values_[m.control]; v.value=m.value;
            if (d && d->type==Primitive::Trigger) v.value.x=double(m.revision);
            if (m.kind==Kind::Event) { v.phase=m.phase; ++v.changes; }
        }
        return true;
    }
    const DiagnosticValue& Get(unsigned slot) const noexcept { return values_[slot<MaxControls ? slot : 0]; }
private:
    std::array<DiagnosticValue,MaxControls> values_{};
};
}
