// SPDX-License-Identifier: GPL-3.0-or-later
// Reusable instruments publish generic values. No endpoint names or OS keys.
class DIOInstrument : Actor
{
    String user_control;
    String Control() { return user_control; }
    Actor LocalPawn() { return players[consoleplayer].mo; }
    bool InReach(Actor pawn) { return pawn && (pawn.Pos.xy-Pos.xy).Length()<100; }
    void DisplayState(String label) { SetState(FindStateByString(label)); }
    void Click() { A_StartSound("switches/normbutn", CHAN_BODY); }
    Default
    {
        Radius 24; Height 64; Scale 0.5;
        +SOLID +NOGRAVITY +NOBLOOD +NODAMAGETHRUST
    }
}
class DIOPulse : DIOInstrument
{
    int flash;
    void Pulse(Actor user)
    {
        if (DoomIO.Publish(user,Control())) { flash=7; DisplayState("On"); Click(); }
    }
    override bool Used(Actor user) { Pulse(user); return true; }
    override int DamageMobj(Actor inflictor,Actor source,int damage,Name mod,int flags,double angle)
    { if (source && source.player) Pulse(source); return 0; }
    override void Tick() { Super.Tick(); if (flash>0 && --flash==0) DisplayState("Spawn"); }
    Default { +SHOOTABLE Health 1000000; }
    States { Spawn: DIBT A -1 Bright; Stop; On: DIBT B -1 Bright; Stop; }
}
class DIOButton : DIOInstrument
{
    int held;
    Actor operatorPawn;
    void Press(Actor user)
    {
        if (DoomIO.Publish(user,Control(),1)) { operatorPawn=user; held=12; DisplayState("On"); Click(); }
    }
    override bool Used(Actor user) { Press(user); return true; }
    override int DamageMobj(Actor inflictor,Actor source,int damage,Name mod,int flags,double angle)
    { if (source && source.player) Press(source); return 0; }
    override void Tick()
    {
        Super.Tick();
        if (held>0 && DoomIO.ValueX(Control())==0) { held=0; operatorPawn=null; DisplayState("Spawn"); }
        if (held>0)
        {
            --held;
            DoomIO.Publish(operatorPawn,Control(),held>0 ? 1 : 0);
            if (!held) { operatorPawn=null; DisplayState("Spawn"); }
        }
    }
    Default { +SHOOTABLE Health 1000000; }
    States { Spawn: DIBB A -1 Bright; Stop; On: DIBB B -1 Bright; Stop; }
}
class DIOToggle : DIOInstrument
{
    override bool Used(Actor user) { DoomIO.Publish(user,Control(),1-DoomIO.ValueX(Control())); Click(); return true; }
    override void Tick() { Super.Tick(); DisplayState(DoomIO.ValueX(Control())>0 ? "On" : "Spawn"); }
    States { Spawn: DITG A -1 Bright; Stop; On: DITG B -1 Bright; Stop; }
}
class DIOAnalog : DIOInstrument
{
    Actor operatorPawn;
    override bool Used(Actor user) { operatorPawn=user; Click(); return true; }
    override void Tick()
    {
        Super.Tick();
        if (operatorPawn)
        {
            double value=0.5;
            if (InReach(operatorPawn) && operatorPawn.player && (operatorPawn.player.cmd.buttons & BT_USE))
                value=Clamp(0.5-operatorPawn.Pitch/60.,0.,1.);
            else operatorPawn=null;
            if (!DoomIO.Publish(LocalPawn(),Control(),value)) operatorPawn=null;
        }
        int step=Clamp(int(DoomIO.ValueX(Control())*8+0.5),0,8);
        DisplayState(String.Format("Level%d",step));
    }
    States
    {
        Spawn: DIAN E -1 Bright; Stop;
        Level0: DIAN A -1 Bright; Stop;
        Level1: DIAN B -1 Bright; Stop;
        Level2: DIAN C -1 Bright; Stop;
        Level3: DIAN D -1 Bright; Stop;
        Level4: DIAN E -1 Bright; Stop;
        Level5: DIAN F -1 Bright; Stop;
        Level6: DIAN G -1 Bright; Stop;
        Level7: DIAN H -1 Bright; Stop;
        Level8: DIAN I -1 Bright; Stop;
    }
}
class DIOPad : DIOInstrument
{
    override void Tick()
    {
        Super.Tick();
        Actor pawn=players[consoleplayer].mo;
        if (!pawn) return;
        Vector2 value=(0,0),offset=pawn.Pos.xy-Pos.xy;
        if (Abs(offset.x)<=160 && Abs(offset.y)<=160 && Abs(pawn.Pos.z-Pos.z)<48)
            value=(Clamp(offset.x/160.,-1.,1.),Clamp(offset.y/160.,-1.,1.));
        DoomIO.Publish(pawn,Control(),value.x,value.y);
    }
    Default { Radius 1; Height 1; +NOBLOCKMAP -SOLID }
    States { Spawn: TNT1 A -1; Stop; }
}
class DIOSelector : DIOInstrument
{
    override bool Used(Actor user) { DoomIO.Publish(user,Control(),(int(DoomIO.ValueX(Control()))+1)%3); Click(); return true; }
    override void Tick() { Super.Tick(); DisplayState(String.Format("Mode%d",int(DoomIO.ValueX(Control())))); }
    States { Spawn: DISE A -1 Bright; Stop; Mode0: DISE A -1 Bright; Stop; Mode1: DISE B -1 Bright; Stop; Mode2: DISE C -1 Bright; Stop; }
}
class DIOMonitor : DIOInstrument
{
    override void Tick()
    {
        Super.Tick();
        double value=Abs(DoomIO.ValueX(Control()));
        if (args[0]==5) value=Abs(value-0.5)*2;
        if (args[0]==7) value/=2;
        int step=Clamp(int(value*8),0,8);
        DisplayState(String.Format("Level%d",step));
    }
    Default { Radius 12; +NOBLOCKMAP -SOLID }
    States
    {
        Spawn: DIDG A -1 Bright; Stop;
        Level0: DIDG A -1 Bright; Stop; Level1: DIDG B -1 Bright; Stop;
        Level2: DIDG C -1 Bright; Stop; Level3: DIDG D -1 Bright; Stop;
        Level4: DIDG E -1 Bright; Stop; Level5: DIDG F -1 Bright; Stop;
        Level6: DIDG G -1 Bright; Stop; Level7: DIDG H -1 Bright; Stop;
        Level8: DIDG I -1 Bright; Stop;
    }
}
class DIOEndpointStation : DIOInstrument
{
    override bool Used(Actor user) { DoomIO.SelectProfile(user,1-DoomIO.SelectedProfile()); Click(); return true; }
    override void Tick() { Super.Tick(); DisplayState(DoomIO.SelectedProfile()==1 ? "External" : "Spawn"); }
    States { Spawn: DIEP A -1 Bright; Stop; External: DIEP B -1 Bright; Stop; }
}
class DIOControlRoom : EventHandler
{
    override void WorldLoaded(WorldEvent e)
    {
        DefineRoom();
    }
    void DefineRoom()
    {
        if (Level.MapName!="DIO01") return;
        DoomIO.Define("trigger.pulse","PULSE",DoomIO.CT_Trigger,0,1);
        DoomIO.Define("button.a","BUTTON A",DoomIO.CT_Button,0,1,0,0,"",0,14);
        DoomIO.Define("button.b","BUTTON B",DoomIO.CT_Button,0,1,0,0,"",0,14);
        DoomIO.Define("toggle.power","LATCH",DoomIO.CT_Toggle,0,1);
        DoomIO.Define("scalar.throttle","ANALOG",DoomIO.CT_Scalar,0,1,0.5,0,"normalized",0,14);
        DoomIO.Define("vector.pad","XY SURFACE",DoomIO.CT_Vector2,-1,1,0,0,"normalized",0,14);
        DoomIO.Define("selector.mode","SELECTOR",DoomIO.CT_Selector,0,2,0,0,"index",3);
    }
    override void WorldTick()
    {
        if (Level.MapName!="DIO01") return;
        if (DoomIO.Count()<8) DefineRoom();
        if (Level.maptime%35==0 && players[consoleplayer].mo) players[consoleplayer].mo.GiveInventory("Clip",200);
    }
    override void RenderOverlay(RenderEvent e)
    {
        if (Level.MapName!="DIO01") return;
        int sw=Screen.GetWidth(),sh=Screen.GetHeight();
        Screen.Dim(0x0a1923,0.78,0,0,sw,int(sh*0.055));
        Screen.DrawText(SmallFont,Font.CR_CYAN,16,8,"DOOM I/O  /  HELIX CONTROL",DTA_VirtualWidth,960,DTA_VirtualHeight,540);
        Screen.DrawText(SmallFont,Font.CR_WHITE,470,8,"F6  INPUT LAB   |   F7  ARM / RELEASE",DTA_VirtualWidth,960,DTA_VirtualHeight,540);
        Screen.Dim(0x0a1923,0.85,0,int(sh*0.83),sw,int(sh*0.065));
        Screen.DrawText(SmallFont,Font.CR_CYAN,16,454,DoomIO.Status(),DTA_VirtualWidth,960,DTA_VirtualHeight,540);
        Screen.DrawText(SmallFont,Font.CR_WHITE,16,470,DoomIO.EndpointStatus(),DTA_VirtualWidth,960,DTA_VirtualHeight,540);
        let pawn=players[consoleplayer].mo;
        bool nearMonitor=pawn && pawn.Pos.x>470 && Abs(pawn.Pos.y-64)<240;
        if (nearMonitor)
        {
            Screen.Dim(0x0a1923,0.9,int(sw*0.62),int(sh*0.16),int(sw*0.36),int(sh*0.45));
            Screen.DrawText(SmallFont,Font.CR_CYAN,610,96,"NATIVE DIAGNOSTIC ENDPOINT",DTA_VirtualWidth,960,DTA_VirtualHeight,540);
            for (int i=1;i<DoomIO.Count();++i)
                Screen.DrawText(SmallFont,Font.CR_WHITE,610,116+i*18,DoomIO.DiagnosticLine(i),DTA_VirtualWidth,960,DTA_VirtualHeight,540);
        }
        if (Level.maptime<280)
        {
            Screen.DrawText(SmallFont,Font.CR_YELLOW,20,56,"SHOOT THE BUTTON BANK. USE THE SWITCHES.",DTA_VirtualWidth,960,DTA_VirtualHeight,540);
            Screen.DrawText(SmallFont,Font.CR_WHITE,20,74,"HOLD USE + LOOK UP/DOWN AT ANALOG. WALK THE XY SURFACE.",DTA_VirtualWidth,960,DTA_VirtualHeight,540);
        }
    }
}
