#pragma once
#include "event_api.h"
namespace SoundAudit {
using Play=void (*)(int,float *,int,const char *,float,float,int,int);
static Play original;
static unsigned count;
static void play(int entity,float *origin,int channel,const char *sample,float volume,float attenuation,int flags,int pitch){
    CallbackScope scope;
    if(ready&&sample&&count<128&&!_strnicmp(sample,"weapons/",8)){
        auto vm=eng->GetViewModel();char text[256];snprintf(text,sizeof(text),"Audio trace: entity=%d view=%s sample=%s",entity,vm&&vm->model?vm->model->name:"none",sample);logLine(text);++count;
    }
    original(entity,origin,channel,sample,volume,attenuation,flags,pitch);
}
static void install(HMODULE hw){
    if(!GetPrivateProfileIntA("diagnostics","audio_trace",0,iniPath)||!eng->pEventAPI)return;
    auto fn=eng->pEventAPI->EV_PlaySound;if(executableInModule(reinterpret_cast<void *>(fn),hw)&&MH_CreateHook(reinterpret_cast<void *>(fn),reinterpret_cast<void *>(play),reinterpret_cast<void **>(&original))==MH_OK)logLine("Audio trace enabled; samples are forwarded unchanged.");
}
}
