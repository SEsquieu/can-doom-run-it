// SPDX-License-Identifier: GPL-3.0-or-later
#include "cdr/control_bus.h"
#include "cdr/diagnostic_adapter.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
using namespace cdr;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#c); return 1; } } while(false)
struct Record { Kind kind; Phase phase; std::string id,reason,session; Value value; std::uint64_t sequence,snapshot; };
struct Recorder : Adapter
{
    bool ready=true,accept=true; unsigned capabilities=0,resets=0;
    std::vector<Record> records;
    const char* Id() const noexcept override { return "test"; }
    unsigned RequiredCapabilities() const noexcept override { return capabilities; }
    bool IsReady() const noexcept override { return ready; }
    bool Send(const Message& m,const Definition* d) noexcept override
    { records.push_back({m.kind,m.phase,d ? d->id : "",std::string(m.reason),std::string(m.session),m.value,m.sequence,m.snapshot}); return accept; }
    void Reset(ResetReason) noexcept override { ++resets; }
};
int main()
{
    Bus bus({},"session-a");
    CHECK(bus.Define({"button.hold","Hold","",Primitive::Button,0,1,{},0,Update::OnChange,14}));
    CHECK(bus.Define({"toggle.power","Power","",Primitive::Toggle}));
    CHECK(bus.Define({"scalar.throttle","Throttle","percent",Primitive::Scalar,0,100,{50,0},0,Update::Continuous,14}));
    CHECK(bus.Define({"vector.pad","Pad","normalized",Primitive::Vector2,-1,1,{},0,Update::Continuous,14}));
    CHECK(bus.Define({"selector.mode","Mode","index",Primitive::Selector,0,2,{},3}));
    CHECK(!bus.Define({"arbitrary\"id","Invalid","",Primitive::Trigger}));
    CHECK(!bus.Define({"selector.bad","Bad","",Primitive::Selector,0,2,{},99}));
    CHECK(!bus.Define({"toggle.power","Conflict","",Primitive::Scalar}));
    Recorder recorder; DiagnosticAdapter diagnostic;
    CHECK(bus.Attach(recorder)); CHECK(bus.Attach(diagnostic)); CHECK(!bus.Attach(recorder));
    CHECK(recorder.records.front().kind==Kind::SnapshotBegin);
    CHECK(recorder.records.back().kind==Kind::SnapshotEnd);
    CHECK(recorder.records[1].session=="session-a");
    CHECK(diagnostic.Get(unsigned(bus.Find("scalar.throttle"))).value.x==50);
    for (std::size_t i=1;i<recorder.records.size();++i) CHECK(recorder.records[i].sequence==recorder.records[i-1].sequence+1);
    const auto emitted=bus.GetCounters().emitted;
    CHECK(bus.Publish("button.hold",{1,0},10)); CHECK(bus.Publish("button.hold",{1,0},11));
    CHECK(bus.GetCounters().emitted==emitted+1); CHECK(recorder.records.back().phase==Phase::Press);
    CHECK(bus.Publish("button.hold",{},12)); CHECK(recorder.records.back().phase==Phase::Release);
    CHECK(bus.Publish("toggle.power",{1,0},12)); CHECK(bus.Publish("toggle.power",{1,0},13));
    CHECK(bus.Publish("scalar.throttle",{500,0},13)); CHECK(bus.GetState(unsigned(bus.Find("scalar.throttle")))->value.x==100);
    CHECK(bus.Publish("vector.pad",{10,-10},13)); CHECK(recorder.records.back().value.x==1 && recorder.records.back().value.y==-1);
    CHECK(!bus.Publish("vector.pad",{std::numeric_limits<double>::quiet_NaN(),0},14));
    CHECK(!bus.Publish("toggle.power",{2,0},14)); CHECK(!bus.Publish("selector.mode",{0.5,0},14));
    CHECK(bus.Publish("selector.mode",{2,0},14)); CHECK(!bus.Publish("selector.mode",{3,0},14));
    CHECK(!bus.Publish("missing",{},14));
    CHECK(bus.Trigger(Control::PlayerUse,14,0));
    bus.Snapshot(15); CHECK(recorder.records.back().kind==Kind::SnapshotEnd);
    CHECK(bus.GetState(unsigned(bus.Find("toggle.power")))->value.x==1);
    CHECK(bus.GetState(unsigned(bus.Find("selector.mode")))->value.x==2);
    CHECK(bus.GetState(unsigned(bus.Find("player.use")))->value.x==0);
    CHECK(bus.GetState(unsigned(bus.Find("player.use")))->revision==1);
    bus.Tick(28); CHECK(bus.GetState(unsigned(bus.Find("scalar.throttle")))->value.x==50);
    CHECK(bus.GetState(unsigned(bus.Find("vector.pad")))->value.x==0);
    CHECK(bus.GetState(unsigned(bus.Find("toggle.power")))->value.x==1); // Latched until a safety boundary.
    Recorder keyboard; keyboard.capabilities=SoftwareKeyboard; CHECK(bus.Attach(keyboard));
    CHECK(keyboard.records.empty()); CHECK(!bus.Grant(PhysicalDevice,true,29));
    CHECK(bus.Grant(SoftwareKeyboard,true,29)); CHECK(!keyboard.records.empty());
    CHECK(bus.Publish("button.hold",{1,0},30)); bus.SetFocused(false,31);
    CHECK(bus.Grants()==0); CHECK(keyboard.resets>0);
    CHECK(bus.GetState(unsigned(bus.Find("button.hold")))->value.x==0);
    CHECK(bus.GetState(unsigned(bus.Find("toggle.power")))->value.x==0);
    CHECK(!bus.Publish("button.hold",{1,0},32)); CHECK(!bus.Grant(SoftwareKeyboard,true,32));
    bus.SetFocused(true,33); CHECK(bus.Grant(SoftwareKeyboard,true,33));
    bus.SetEnabled(false,34); CHECK(bus.Grants()==0); CHECK(!bus.Publish("button.hold",{1,0},35));
    bus.SetEnabled(true,36); CHECK(bus.Grants()==0);
    CHECK(bus.Grant(SoftwareKeyboard,true,36)); bus.Reset(ResetReason::MapChange,37); CHECK(bus.Grants()==0);
    keyboard.ready=false; CHECK(bus.Trigger(Control::PlayerUse,38,0)); // Diagnostic remains available.
    bus.Reset(ResetReason::Shutdown,39); CHECK(keyboard.resets>0);
    char tiny[8]{}; Message m; m.session="test"; CHECK(Encode(m,nullptr,tiny,sizeof(tiny))==0);
    Bus other({},"session-b"); CHECK(other.Session()!=bus.Session()); CHECK(NewSession()!=NewSession());
    Bus bounded;
    for (unsigned i=1;i<MaxControls;++i) CHECK(bounded.Define({"c."+std::to_string(i),"Control","",Primitive::Toggle}));
    CHECK(!bounded.Define({"overflow","Overflow","",Primitive::Toggle}));
    std::puts("PASS: all six primitives, metadata, leases, snapshots, sessions, permissions, lifecycle, bounds");
}
