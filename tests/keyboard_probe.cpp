// SPDX-License-Identifier: GPL-3.0-or-later
#include "cdr/keyboard_adapter.h"
#include <chrono>
#include <cstdio>
#include <thread>
using namespace cdr;
int main()
{
    KeyboardAdapter keyboard(NativeKeyboardEndpoint());
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while (!keyboard.IsReady() && std::chrono::steady_clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    if (!keyboard.PrepareArm()) { std::fprintf(stderr,"Input Lab window unavailable\n"); return 2; }
    Bus bus; bus.Define({"trigger.pulse","Pulse","",Primitive::Trigger});
    bus.Define({"button.a","A","",Primitive::Button}); bus.Define({"button.b","B","",Primitive::Button});
    bus.Define({"toggle.power","Power","",Primitive::Toggle});
    bus.Define({"scalar.throttle","Throttle","",Primitive::Scalar,0,1,{0.5,0}});
    bus.Define({"vector.pad","Pad","",Primitive::Vector2,-1,1});
    bus.Define({"selector.mode","Mode","",Primitive::Selector,0,2,{},3});
    bus.Attach(keyboard); bus.Grant(SoftwareKeyboard,true,0);
    bus.Publish("button.a",{1,0},1); bus.Publish("toggle.power",{1,0},1);
    bus.Publish("vector.pad",{1,1},1); bus.Publish("selector.mode",{2,0},1);
    for (unsigned i=0;i<15;++i) { keyboard.Tick(i); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    bus.Publish("trigger.pulse",{},16);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    bus.Reset(ResetReason::Shutdown,16); keyboard.Stop();
    return 0;
}
