#pragma once
#ifdef clamp
#undef clamp
#endif
namespace Speedometer {
static bool enabled=true;
static int rgb[3]={255,255,255},size=32,xPercent=50,yPercent=80;
static bool outline=true;
static int outlineWidth=1,outlineRgb[3]={0,0,0};
static float speed;
static ULONGLONG sampleTick;
static bool valid;
static void reset(){speed=0;sampleTick=0;valid=false;}
static void defaults(){enabled=true;size=32;xPercent=50;yPercent=80;for(int &c:rgb)c=255;outline=true;outlineWidth=1;for(int &c:outlineRgb)c=0;}
static void observe(const local_state_t *state,ULONGLONG tick){
    if(!state||state->client.deadflag||state->client.health<=0||state->client.iuser1){reset();return;}
    float vx=state->client.velocity[0],vy=state->client.velocity[1];
    if(!std::isfinite(vx)||!std::isfinite(vy)){reset();return;}
    speed=std::hypot(vx,vy);if(!std::isfinite(speed)){reset();return;}
    sampleTick=tick;valid=true;
}
static bool fresh(ULONGLONG tick){return valid&&tick>=sampleTick&&tick-sampleTick<=1000;}
static void save(){
    const char *keys[]={"speedometer","speedometer_size","speedometer_x","speedometer_y","speedometer_rgb_0","speedometer_rgb_1","speedometer_rgb_2"};
    int values[]={enabled,size,xPercent,yPercent,rgb[0],rgb[1],rgb[2]};
    for(int i=0;i<7;++i){char text[24];snprintf(text,sizeof(text),"%d",values[i]);WritePrivateProfileStringA("interface",keys[i],text,iniPath);}
    const char *outlineKeys[]={"speedometer_outline","speedometer_outline_width","speedometer_outline_rgb_0","speedometer_outline_rgb_1","speedometer_outline_rgb_2"};
    int outlineValues[]={outline,outlineWidth,outlineRgb[0],outlineRgb[1],outlineRgb[2]};
    for(int i=0;i<5;++i){char text[24];snprintf(text,sizeof(text),"%d",outlineValues[i]);WritePrivateProfileStringA("interface",outlineKeys[i],text,iniPath);}
}
static void load(){
    auto setting=[](const char *key,int fallback,int lo,int hi){return std::clamp(static_cast<int>(GetPrivateProfileIntA("interface",key,fallback,iniPath)),lo,hi);};
    enabled=setting("speedometer",1,0,1)!=0;size=setting("speedometer_size",32,12,96);
    xPercent=setting("speedometer_x",50,0,100);yPercent=setting("speedometer_y",80,0,100);
    for(int i=0;i<3;++i){char key[32];snprintf(key,sizeof(key),"speedometer_rgb_%d",i);rgb[i]=setting(key,255,0,255);}
    outline=setting("speedometer_outline",1,0,1)!=0;outlineWidth=setting("speedometer_outline_width",1,1,3);
    for(int i=0;i<3;++i){char key[40];snprintf(key,sizeof(key),"speedometer_outline_rgb_%d",i);outlineRgb[i]=setting(key,0,0,255);}
}
}
