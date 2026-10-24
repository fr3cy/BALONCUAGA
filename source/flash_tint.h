#pragma once
#include "screenfade.h"
// Flashbang colour: the full-screen fade a flashbang starts is given the colour chosen in the
// menu. Only the colour changes; alpha, hold and fade times stay the server's, so the flash
// blinds exactly as long and as strongly as before.
namespace FlashTint {
static constexpr int FadeOut=0x0001,Modulate=0x0002,StayOut=0x0004;   // FFADE_OUT, FFADE_MODULATE, FFADE_STAYOUT
static bool own;static float ownEnd,ownReset;static byte original[3];   // the fade this plugin coloured, and the server's colour of it
static bool incoming;                                                  // the ScreenFade message just handled is a flashbang's
static int logged;
// A flashbang blinds with a fade that starts at full strength and fades away (no FFADE_OUT,
// MODULATE or STAYOUT), bright and strong (alpha 200 or 255). Servers and their plugins
// (Hide'n'Seek, coloured flashes) often send it in their own colour, so the colour is not
// part of the test. Dark fades (to black on death), weak ones (hit flashes, freezing) and
// modulated ones (night vision) are left as they are.
static bool flashbang(int flags,int r,int g,int b,int a){return !(flags&(FadeOut|Modulate|StayOut))&&a>=150&&std::max({r,g,b})>=96;}
// Called with the raw ScreenFade message before the engine takes it: duration and hold
// (unsigned 4.12 fixed point), flags, then r, g, b, alpha.
static void message(const void *data,int size){
    incoming=false;if(!data||size<10)return;const byte *p=static_cast<const byte *>(data);
    const int duration=p[0]|p[1]<<8,hold=p[2]|p[3]<<8,flags=static_cast<int16_t>(p[4]|p[5]<<8),r=p[6],g=p[7],b=p[8],a=p[9];
    incoming=flashbang(flags,r,g,b,a);
    if(logged<6){++logged;char line[192];snprintf(line,sizeof(line),"Flash colour: ScreenFade %d,%d,%d alpha %d flags %d, %.2f s + hold %.2f s: %s.",r,g,b,a,flags,duration/4096.f,hold/4096.f,incoming?"a flashbang, coloured":"another fade, kept");logLine(line);}
}
static void apply(){
    const bool fresh=incoming;incoming=false;
    if(!eng||!eng->pfnGetScreenFade||!eng->pfnSetScreenFade)return;
    screenfade_t fade={};eng->pfnGetScreenFade(&fade);
    if((fade.fadeFlags&Modulate)||!fade.fadealpha){own=false;return;}
    if(!(own&&fade.fadeEnd==ownEnd&&fade.fadeReset==ownReset)){
        // A new fade: a flashbang by its message, or (whatever brought it) plain white.
        const bool white=fade.fader==255&&fade.fadeg==255&&fade.fadeb==255;
        if(!fresh&&!white){own=false;return;}                         // a coloured fade from the map or server
        if(!Hud::flashTintOn){own=false;return;}
        own=true;ownEnd=fade.fadeEnd;ownReset=fade.fadeReset;original[0]=fade.fader;original[1]=fade.fadeg;original[2]=fade.fadeb;
    }
    // Ours: the menu colour, or (option turned off meanwhile) the server's own again.
    byte r=original[0],g=original[1],b=original[2];
    if(Hud::flashTintOn){r=static_cast<byte>(Hud::flashRgb[0]);g=static_cast<byte>(Hud::flashRgb[1]);b=static_cast<byte>(Hud::flashRgb[2]);}
    if(fade.fader==r&&fade.fadeg==g&&fade.fadeb==b)return;
    fade.fader=r;fade.fadeg=g;fade.fadeb=b;eng->pfnSetScreenFade(&fade);
    static bool noted;if(!noted&&Hud::flashTintOn){noted=true;logLine("Flash colour: flashbang fade recoloured (alpha and timing unchanged).");}
}
}
