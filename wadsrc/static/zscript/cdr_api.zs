// SPDX-License-Identifier: GPL-3.0-or-later
// Native control values only. No networking, files, OS keys, or authority grants.
struct DoomIO native
{
    enum EPrimitive { CT_Trigger, CT_Button, CT_Toggle, CT_Scalar, CT_Vector2, CT_Selector };
    native static play bool Define(String id, String label, int type, double minimum, double maximum,
        double neutralX = 0, double neutralY = 0, String units = "", int optionCount = 0, int staleTicks = 0);
    native static play bool Publish(Actor operatorActor, String id, double x = 1, double y = 0);
    native static double ValueX(String id);
    native static double ValueY(String id);
    native static String ControlId(int slot);
    native static int Count();
    native static String DiagnosticLine(int slot);
    native static String Status();
}
