// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_io.h"
#include "control_bus.h"
#include "diagnostic_adapter.h"
#include "udp_output.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "g_levellocals.h"
#include "playsim/actor.h"
#include "playsim/d_player.h"
#include "printf.h"
#include "vm.h"
#include <cinttypes>

CVAR(Bool, cdr_io_enabled, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
namespace
{
struct LocalOutput
{
    cdr::LoopbackUdpOutput output{cdr::DefaultOutputPort,true};
    cdr::DiagnosticAdapter diagnostic;
    cdr::Bus bus;
    LocalOutput() { bus.Attach(diagnostic); bus.Attach(output); }
};
LocalOutput* activeOutput=nullptr;
LocalOutput& GetLocalOutput()
{
    static LocalOutput state;
    activeOutput=&state;
    return state;
}
bool LiveOperator(const AActor* actor)
{
    return actor && actor==players[consoleplayer].mo && actor->player==&players[consoleplayer] &&
        !demoplayback && !netgame && gamestate==GS_LEVEL && cdr_io_enabled;
}
}
void cdr::PublishPlayerUse(const player_t* player)
{
    if (!cdr_io_enabled || player!=&players[consoleplayer] || !LiveOperator(player->mo)) return;
    GetLocalOutput().bus.Trigger(Control::PlayerUse,std::uint64_t(gametic),std::uint32_t(consoleplayer));
}
void cdr::UpdateRuntime(bool focused,bool live,std::uint64_t tick)
{
    auto& state=GetLocalOutput();
    state.bus.SetEnabled(cdr_io_enabled,tick);
    state.bus.SetFocused(focused && live,tick);
    state.bus.Tick(tick);
    if (state.output.TakeSnapshotRequest()) state.bus.Snapshot(tick);
}
void cdr::ShutdownRuntime()
{
    if (activeOutput) { activeOutput->bus.Reset(ResetReason::Shutdown,std::uint64_t(gametic)); activeOutput->bus.SetEnabled(false,std::uint64_t(gametic)); }
}
CCMD(cdr_io_status)
{
    const auto& state=GetLocalOutput(); const auto& c=state.bus.GetCounters();
    Printf("Native I/O: enabled=%d ready=%d UDP 127.0.0.1:%u emitted=%" PRIu64 " submitted=%" PRIu64 " dropped=%" PRIu64 " session=%s controls=%u\n",
        int(bool(cdr_io_enabled)),int(state.output.IsReady()),unsigned(cdr::DefaultOutputPort),c.emitted,c.submitted,c.dropped,state.bus.Session().c_str(),state.bus.Count());
}
DEFINE_ACTION_FUNCTION(_DoomIO, Define)
{
    PARAM_PROLOGUE; PARAM_STRING(id); PARAM_STRING(label); PARAM_INT(type);
    PARAM_FLOAT(minimum); PARAM_FLOAT(maximum); PARAM_FLOAT(neutralX); PARAM_FLOAT(neutralY);
    PARAM_STRING(units); PARAM_INT(states); PARAM_INT(staleTicks);
    cdr::Definition d{id.GetChars(),label.GetChars(),units.GetChars(),cdr::Primitive(type),minimum,maximum,{neutralX,neutralY},unsigned(states),
        type==3 || type==4 ? cdr::Update::Continuous : cdr::Update::OnChange,std::uint64_t(staleTicks)};
    ACTION_RETURN_BOOL(GetLocalOutput().bus.Define(d));
}
DEFINE_ACTION_FUNCTION(_DoomIO, Publish)
{
    PARAM_PROLOGUE; PARAM_OBJECT(operatorActor,AActor); PARAM_STRING(id); PARAM_FLOAT(x); PARAM_FLOAT(y);
    ACTION_RETURN_BOOL(LiveOperator(operatorActor) && GetLocalOutput().bus.Publish(id.GetChars(),{x,y},std::uint64_t(gametic),unsigned(consoleplayer)));
}
DEFINE_ACTION_FUNCTION(_DoomIO, ValueX)
{
    PARAM_PROLOGUE; PARAM_STRING(id); auto& bus=GetLocalOutput().bus; const auto* value=bus.GetState(unsigned(bus.Find(id.GetChars())));
    ACTION_RETURN_FLOAT(value ? value->value.x : 0);
}
DEFINE_ACTION_FUNCTION(_DoomIO, ValueY)
{
    PARAM_PROLOGUE; PARAM_STRING(id); auto& bus=GetLocalOutput().bus; const auto* value=bus.GetState(unsigned(bus.Find(id.GetChars())));
    ACTION_RETURN_FLOAT(value ? value->value.y : 0);
}
DEFINE_ACTION_FUNCTION(_DoomIO, ControlId)
{
    PARAM_PROLOGUE; PARAM_INT(slot); const auto* d=GetLocalOutput().bus.GetDefinition(unsigned(slot));
    ACTION_RETURN_STRING(d ? d->id.c_str() : "");
}
DEFINE_ACTION_FUNCTION(_DoomIO, Count)
{
    PARAM_PROLOGUE; ACTION_RETURN_INT(GetLocalOutput().bus.Count());
}
DEFINE_ACTION_FUNCTION(_DoomIO, DiagnosticLine)
{
    PARAM_PROLOGUE; PARAM_INT(slot); auto& state=GetLocalOutput(); const auto* d=state.bus.GetDefinition(unsigned(slot));
    if (!d) ACTION_RETURN_STRING("");
    const auto& v=state.diagnostic.Get(unsigned(slot));
    FString line;
    if (d->type==cdr::Primitive::Vector2) line.Format("%s  %+.2f  %+.2f",d->label.c_str(),v.value.x,v.value.y);
    else if (d->type==cdr::Primitive::Trigger) line.Format("%s  %.0f pulses",d->label.c_str(),v.value.x);
    else if (d->type==cdr::Primitive::Button || d->type==cdr::Primitive::Toggle) line.Format("%s  %s",d->label.c_str(),v.value.x ? "ON" : "OFF");
    else if (d->type==cdr::Primitive::Selector) line.Format("%s  %.0f / %u",d->label.c_str(),v.value.x+1,d->states);
    else line.Format("%s  %.2f %s",d->label.c_str(),v.value.x,d->units.c_str());
    ACTION_RETURN_STRING(line);
}
DEFINE_ACTION_FUNCTION(_DoomIO, Status)
{
    PARAM_PROLOGUE; auto& state=GetLocalOutput(); FString text;
    text.Format("%s | %u controls | UDP %s",state.bus.Enabled() ? "NATIVE BUS ONLINE" : "OUTPUT DISABLED",state.bus.Count(),state.output.IsReady() ? "READY" : "OFFLINE");
    ACTION_RETURN_STRING(text);
}
