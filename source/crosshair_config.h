#pragma once
namespace CrossConfig {
static int behavior=0,length=8,thickness=2,gap=4,preset=0,transparency=0,outlineWidth=1;
static bool dot=false,outline=true;
static int rgb[3]={50,250,50};
static const char *behaviors[]={u8"Статичный",u8"Динамичный",u8"Гибридный"};
static const char *colors[]={u8"Зелёный",u8"Белый",u8"Голубой",u8"Жёлтый",u8"Красный",u8"Фиолетовый",u8"Оранжевый",u8"Синий",u8"Свой RGB"};
static const int values[][3]={{50,250,50},{255,255,255},{70,220,255},{255,225,60},{255,65,65},{170,100,255},{255,150,45},{100,160,255}};
static void reset(){behavior=0;length=8;thickness=2;gap=4;preset=0;transparency=0;outlineWidth=1;dot=false;outline=true;for(int i=0;i<3;++i)rgb[i]=values[0][i];}
static void save(){const char *keys[]={"cs2_behavior","cs2_length","cs2_thickness","cs2_gap","cs2_color","cs2_transparency","cs2_outline_width","cs2_dot","cs2_outline","cs2_red","cs2_green","cs2_blue"};int vals[]={behavior,length,thickness,gap,preset,transparency,outlineWidth,dot,outline,rgb[0],rgb[1],rgb[2]};char text[24];for(int i=0;i<12;++i){snprintf(text,sizeof(text),"%d",vals[i]);WritePrivateProfileStringA("crosshair",keys[i],text,iniPath);}}
static void load(){auto read=[](const char *key,int fallback,int lo,int hi){return std::max(lo,std::min(hi,static_cast<int>(GetPrivateProfileIntA("crosshair",key,fallback,iniPath))));};
    behavior=read("cs2_behavior",0,0,2);length=read("cs2_length",8,0,40);thickness=read("cs2_thickness",2,1,10);gap=read("cs2_gap",4,0,30);preset=read("cs2_color",0,0,8);transparency=read("cs2_transparency",0,0,100);outlineWidth=read("cs2_outline_width",1,1,3);dot=read("cs2_dot",0,0,1);outline=read("cs2_outline",1,0,1);rgb[0]=read("cs2_red",50,0,255);rgb[1]=read("cs2_green",250,0,255);rgb[2]=read("cs2_blue",50,0,255);
    if(GetPrivateProfileIntA("crosshair","cs2_revision",0,iniPath)<36){crossStyle=12;WritePrivateProfileStringA("crosshair","cs2_revision","36",iniPath);WritePrivateProfileStringA("crosshair","style","12",iniPath);}
}
struct Rect {int x,y,w,h;bool border;};
static int alpha(){return (100-transparency)*255/100;}
static float expansion(float speed,float shooting){return behavior==0?0.f:shooting+(behavior==1?std::min(10.f,std::max(0.f,speed)/25.f):0.f);}
static std::vector<Rect> geometry(int cx,int cy,float spread){
    std::vector<Rect> out;int g=gap+static_cast<int>(std::round(std::max(0.f,spread))),t=thickness,half=t/2;
    auto add=[&](int x,int y,int w,int h){if(w<=0||h<=0)return;if(outline)out.push_back({x-outlineWidth,y-outlineWidth,w+outlineWidth*2,h+outlineWidth*2,true});out.push_back({x,y,w,h,false});};
    add(cx-g-length,cy-half,length,t);add(cx+g,cy-half,length,t);add(cx-half,cy-g-length,t,length);add(cx-half,cy+g,t,length);if(dot)add(cx-half,cy-half,t,t);return out;
}
}
