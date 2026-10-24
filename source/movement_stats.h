#pragma once
#include <cstdint>
// Local movement statistics. This contains no server enforcement or AMXX runtime.
namespace MovementStats {
using Count=uint64_t;
struct Distribution {
    Count total=0,fog[7]={}; // 0..5 and 6+; the latter keeps the denominator honest.
    void add(int frames){++total;++fog[std::clamp(frames,0,6)];}
};
struct Totals {Distribution bhop,duck;Count ideal=0,distributed=0,jumpbug=0;};
struct Tracker {
    bool primed=false,distributionArmed=false,jumpbugWindow=false,pendingJumpbug=false,duckedOnGround=false;
    int frames=0,airFrames=0,lastGroundFog=0,alternations=0;
    float speed=0,x=0,y=0,z=0;
};
static Totals totals;
static Tracker tracker;
// Event metadata for the HUD; it never changes any CA counter or denominator.
struct Attempt {bool ideal=false;int fog=0;};
static Attempt lastAttempt;
static Count attemptSequence=0;
static ULONGLONG periodStarted=GetTickCount64();
static double percent(Count count,Count total){return total?100.0*static_cast<double>(count)/static_cast<double>(total):0;}
static void resetTracking(){tracker={};}
static void clear(){totals={};resetTracking();periodStarted=GetTickCount64();}
// client_analyzer183_: fwCmdStart @209064; CheckBhop @17128.
static bool isIdeal(int fog,float speed,float previousSpeed,float maximum,bool standingDuckPress){
    if(standingDuckPress||!std::isfinite(maximum)||maximum<=0)return false;
    return (speed<maximum*1.2f&&(fog==1||(fog>=2&&previousSpeed>maximum*1.2f)))||
           (speed>=maximum*1.38f&&fog==1);
}
static void observe(const MovementHud::Frame &before,const MovementHud::Frame &after,bool fresh){
    if(!fresh)return;
    float speed=std::hypot(before.vx,before.vy);
    if(!before.alive||before.type==MOVETYPE_NOCLIP||(static_cast<uint32_t>(before.flags)&0x80001000u)||
       !std::isfinite(speed)||!std::isfinite(after.vz)||!std::isfinite(before.dt)||before.dt<=0||before.dt>.25f||
       !std::isfinite(before.x)||!std::isfinite(before.y)||!std::isfinite(before.z)||
       !std::isfinite(after.x)||!std::isfinite(after.y)||!std::isfinite(after.z)||
       std::hypot(after.x-before.x,after.y-before.y)>128||std::fabs(after.z-before.z)>128||
       (tracker.primed&&(std::hypot(before.x-tracker.x,before.y-tracker.y)>128||std::fabs(before.z-tracker.z)>128))){resetTracking();return;}
    if(!tracker.primed){tracker.primed=true;tracker.speed=speed;}
    // Match fwCmdStart, not the HUD's post-movement takeoff detector. The engine
    // FL_ONGROUND flag and oldbuttons are authoritative for this command;
    // pm->onground may be a stale working value before PM_CategorizePosition.
    bool ground=(before.flags&FL_ONGROUND)!=0;
    bool crouched=(before.flags&FL_DUCKING)!=0;
    bool jumpPressed=(before.buttons&IN_JUMP)&&!(before.oldbuttons&IN_JUMP);
    bool duckPressed=(before.buttons&IN_DUCK)&&!(before.oldbuttons&IN_DUCK);
    int previousAir=tracker.airFrames;
    tracker.frames=ground?std::min(10000,tracker.frames+1):0;
    tracker.airFrames=ground?0:std::min(10000,tracker.airFrames+1);
    int fog=tracker.frames;
    if(ground||before.type==MOVETYPE_FLY)tracker.jumpbugWindow=false;
    if(ground){
        tracker.lastGroundFog=fog;
        if(jumpPressed)tracker.duckedOnGround=false;
        else if(duckPressed)tracker.duckedOnGround=true;
    }
    bool standingDuckPress=ground&&fog<=5&&duckPressed&&!crouched;
    bool jumpbug=tracker.pendingJumpbug;
    // CheckBhop @12228: attempts count even if this command does not leave the
    // ground. FOG 1 requires >=3 preceding airborne commands; FOG 2..5 do not.
    bool bhop=jumpPressed&&((fog==1&&previousAir>=3)||(fog>=2&&fog<=5));
    if(jumpbug||bhop){
        totals.bhop.add(fog);
        bool ideal=!jumpbug&&isIdeal(fog,speed,tracker.speed,before.maxSpeed,standingDuckPress);
        lastAttempt={ideal,fog};++attemptSequence;
        if(jumpbug)++totals.jumpbug;
        else if(ideal)++totals.ideal;
        tracker.distributionArmed=true;
        // Do not reset the input pattern here: CheckBhop keeps the existing
        // consecutive-transition count across this event.
    }
    // CheckGstrafe @66992: standing +duck edges in the first five ground
    // commands. HUD confirmation/release timing is deliberately independent.
    if(ground&&fog<=5&&duckPressed&&!crouched)totals.duck.add(fog);
    // CheckBhop @23944: compare against the engine oldbuttons, not the last raw
    // input sampled by this ASI. Long ground contacts and ground duck invalidate
    // the regular-bhop window, but the plugin retains the jumpbug window.
    if(((before.buttons^before.oldbuttons)&IN_JUMP)!=0){
        tracker.alternations=std::min(10000,tracker.alternations+1);
        if(tracker.distributionArmed&&tracker.alternations>=8&&
           (tracker.jumpbugWindow||(tracker.lastGroundFog<=5&&!tracker.duckedOnGround))){
            ++totals.distributed;tracker.distributionArmed=false;tracker.alternations=0;
        }
    }else tracker.alternations=0;
    // fwCmdEnd writes the jumpbug flag after movement. CheckBhop consumes it in
    // the NEXT fwCmdStart, using that command's FOG rather than forcing FOG 0.
    tracker.pendingJumpbug=!ground&&before.vz<0&&after.vz>0&&(before.buttons&IN_JUMP)&&
        std::isfinite(after.jumpTimer)&&std::fabs(after.jumpTimer-1315.7894287109375f)<.01f;
    if(tracker.pendingJumpbug)tracker.jumpbugWindow=true;
    tracker.speed=speed;tracker.x=after.x;tracker.y=after.y;tracker.z=after.z;
}
}
