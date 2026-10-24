#pragma once
// Menu language: Auto (follows the Windows UI language), English or Russian.
// Every menu string goes through L(russian, english).
namespace Lang {
enum { Auto, English, Russian, Count };
static int mode=Auto;
static bool english(){
    if(mode==English)return true;
    if(mode==Russian)return false;
    return PRIMARYLANGID(GetUserDefaultUILanguage())!=LANG_RUSSIAN;
}
static const char *L(const char *russian,const char *english){return Lang::english()?english:russian;}
static void load(){mode=std::clamp(static_cast<int>(GetPrivateProfileIntA("interface","language",Auto,iniPath)),0,Count-1);}
static void save(){char value[12];snprintf(value,sizeof(value),"%d",mode);WritePrivateProfileStringA("interface","language",value,iniPath);}
}
using Lang::L;
