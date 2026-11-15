// SPDX-License-Identifier: GPL-3.0-or-later
#include "game_io.h"
#include "control_bus.h"
#include "diagnostic_adapter.h"
#include "udp_output.h"
#include "keyboard_adapter.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "g_levellocals.h"
#include "playsim/actor.h"
#include "playsim/d_player.h"
#include "printf.h"
#include "vm.h"
#include "keydef.h"
#include "cmdlib.h"
#include "m_argv.h"
#include <chrono>
#include <cinttypes>

CVAR(Bool, cdr_io_enabled, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Int, cdr_endpoint_profile, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
namespace
{
struct LocalOutput
{
    cdr::LoopbackUdpOutput output{cdr::DefaultOutputPort,true};
    cdr::DiagnosticAdapter diagnostic;
    cdr::KeyboardAdapter keyboard{cdr::NativeKeyboardEndpoint()};
    cdr::Bus bus;
    LocalOutput() { bus.Attach(diagnostic); bus.Attach(output); bus.Attach(keyboard); }
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
        !demoplayback && !netgame && !multiplayer && gamestate==GS_LEVEL && cdr_io_enabled;
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
    if ((state.bus.Grants() & SoftwareKeyboard) && (!state.keyboard.IsReady() || cdr_endpoint_profile!=1))
        state.bus.Grant(SoftwareKeyboard,false,tick);
    state.bus.Tick(tick);
    if (state.output.TakeSnapshotRequest()) state.bus.Snapshot(tick);
}
void cdr::ShutdownRuntime()
{
    if (activeOutput) { activeOutput->bus.Reset(ResetReason::Shutdown,std::uint64_t(gametic)); activeOutput->bus.SetEnabled(false,std::uint64_t(gametic)); }
}
void cdr::BeginMap() { GetLocalOutput().bus.BeginMap(std::uint64_t(gametic)); }
void cdr::ConfigureFirstRun(FArgs* args)
{
    if (args->CheckParm("-iwad") || args->CheckParm("-file") || args->CheckParm("-playdemo") || args->CheckParm("-timedemo")) return;
    FString assets=NicePath("$PROGDIR/freedoom2.wad");
    FString room=NicePath("$PROGDIR/doom-io.pk3");
    if (!FileExists(assets.GetChars()) || !FileExists(room.GetChars())) return;
    args->AppendArg("-iwad"); args->AppendArg(assets.GetChars());
    args->AppendArg("-file"); args->AppendArg(room.GetChars());
    if (!args->CheckParm("+map") && !args->CheckParm("-warp")) { args->AppendArg("+map"); args->AppendArg("DIO01"); }
    args->AppendArg("-noautoload");
    if (!args->CheckParm("+screenblocks")) { args->AppendArg("+screenblocks"); args->AppendArg("12"); }
    if (!args->CheckParm("+crosshair")) { args->AppendArg("+crosshair"); args->AppendArg("1"); }
}
namespace
{
bool OpenLab()
{
    auto& state=GetLocalOutput(); cdr_endpoint_profile=1;
    if (state.keyboard.Available()) return true;
    static auto last=std::chrono::steady_clock::time_point::min();
    const auto now=std::chrono::steady_clock::now();
    if (last!=std::chrono::steady_clock::time_point::min() && now-last<std::chrono::seconds(2)) return false;
    last=now;
#ifdef _WIN32
    FString executable=NicePath("$PROGDIR/doom-io-input-lab.exe");
#else
    FString executable=NicePath("$PROGDIR/doom-io-input-lab");
#endif
    return cdr::LaunchInputLab(executable.GetChars());
}
bool ToggleArm()
{
    auto& state=GetLocalOutput();
    if (state.bus.Grants() & cdr::SoftwareKeyboard) return state.bus.Grant(cdr::SoftwareKeyboard,false,std::uint64_t(gametic));
    if (cdr_endpoint_profile!=1 || !cdr::KeyboardProfileCompatible(state.bus) || !state.keyboard.PrepareArm()) return false;
    return state.bus.Grant(cdr::SoftwareKeyboard,true,std::uint64_t(gametic));
}
}
bool cdr::RuntimeKey(int key)
{
    if (!activeOutput || activeOutput->bus.Find("vector.pad")<0 || gamestate!=GS_LEVEL) return false;
    if (key==KEY_F6) { OpenLab(); return true; }
    if (key==KEY_F7) { ToggleArm(); return true; }
    return false;
}
UNSAFE_CCMD(cdr_open_lab) { OpenLab(); }
UNSAFE_CCMD(cdr_arm) { Printf("Keyboard authority: %s\n",ToggleArm() ? "changed" : "unavailable (check profile, focus and endpoint)"); }
CCMD(cdr_io_status)
{
    const auto& state=GetLocalOutput(); const auto& c=state.bus.GetCounters();
    Printf("Native I/O: enabled=%d ready=%d focused=%d grants=%u UDP 127.0.0.1:%u emitted=%" PRIu64 " submitted=%" PRIu64 " dropped=%" PRIu64 " session=%s controls=%u\n",
        int(bool(cdr_io_enabled)),int(state.output.IsReady()),int(state.bus.Focused()),state.bus.Grants(),unsigned(cdr::DefaultOutputPort),c.emitted,c.submitted,c.dropped,state.bus.Session().c_str(),state.bus.Count());
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
DEFINE_ACTION_FUNCTION(_DoomIO, SelectedProfile)
{
    PARAM_PROLOGUE; ACTION_RETURN_INT(cdr_endpoint_profile==1 ? 1 : 0);
}
DEFINE_ACTION_FUNCTION(_DoomIO, SelectProfile)
{
    PARAM_PROLOGUE; PARAM_OBJECT(operatorActor,AActor); PARAM_INT(profile);
    if (!LiveOperator(operatorActor) || profile<0 || profile>1) ACTION_RETURN_BOOL(false);
    GetLocalOutput().bus.Grant(cdr::SoftwareKeyboard,false,std::uint64_t(gametic)); cdr_endpoint_profile=profile;
    ACTION_RETURN_BOOL(true);
}
DEFINE_ACTION_FUNCTION(_DoomIO, EndpointStatus)
{
    PARAM_PROLOGUE; auto& state=GetLocalOutput(); FString text;
    if (cdr_endpoint_profile!=1) text="PROFILE: NATIVE DIAGNOSTIC | Use endpoint station to select keyboard";
    else text.Format("PROFILE: KEYBOARD LAB | %s | %s",state.keyboard.Status(),state.bus.Grants() & cdr::SoftwareKeyboard ? "ARMED" : "DISARMED - F7 TO AUTHORIZE");
    ACTION_RETURN_STRING(text);
}
