#pragma once
// The plugin's own INI is kept in memory. Writing a value only changes the copy in
// memory; the whole file is written once, on the background I/O thread, shortly
// after values stop changing. Before, WritePrivateProfileString rewrote the file
// for every key (~170 keys per save) on the render thread, and sliders and colour
// pickers saved on every frame of a drag, so the game stuttered while editing.
// Other INI files (config profiles) still go straight to Windows.
#include <string>
#include <vector>
#include <unordered_map>
#include "background_io.h"
namespace IniCache {
static SRWLOCK lock=SRWLOCK_INIT;
static std::string path;
static std::vector<std::string> lines;
static std::unordered_map<std::string,size_t> index;     // "section\nkey" in lower case -> line
static bool loaded,dirty,writing,bom;
static bool deferred;                                     // false: every change is written at once
static ULONGLONG changedTick;
static WIN32_FILE_ATTRIBUTE_DATA stamp;static bool stampValid;
static unsigned failures;
static constexpr ULONGLONG quietMs=600;

static std::string lower(std::string s){for(char &c:s)c=static_cast<char>(tolower(static_cast<unsigned char>(c)));return s;}
static std::string trim(const std::string &s){size_t a=s.find_first_not_of(" \t"),b=s.find_last_not_of(" \t");return a==std::string::npos?std::string():s.substr(a,b-a+1);}
static bool header(const std::string &line,std::string &name){
    std::string t=trim(line);if(t.size()<2||t[0]!='[')return false;size_t end=t.find(']');if(end==std::string::npos)return false;
    name=trim(t.substr(1,end-1));return true;
}
static bool entry(const std::string &line,std::string &key,std::string &value){
    size_t eq=line.find('=');if(eq==std::string::npos)return false;key=trim(line.substr(0,eq));if(key.empty()||key[0]=='['||key[0]==';')return false;
    value=trim(line.substr(eq+1));return true;
}
static void reindex(){
    index.clear();std::string section;
    for(size_t i=0;i<lines.size();++i){std::string name,key,value;
        if(header(lines[i],name)){section=lower(name);continue;}
        if(entry(lines[i],key,value))index.emplace(section+'\n'+lower(key),i);}
}
static bool sameFile(const WIN32_FILE_ATTRIBUTE_DATA &a,bool aValid,const WIN32_FILE_ATTRIBUTE_DATA &b,bool bValid){
    if(aValid!=bValid)return false;if(!aValid)return true;
    return a.nFileSizeLow==b.nFileSizeLow&&a.nFileSizeHigh==b.nFileSizeHigh&&CompareFileTime(&a.ftLastWriteTime,&b.ftLastWriteTime)==0;
}
static void readStamp(){stampValid=GetFileAttributesExA(path.c_str(),GetFileExInfoStandard,&stamp)!=0;}
static void load(const char *file){
    path=file;lines.clear();bom=false;dirty=false;loaded=true;std::string data;
    HANDLE h=CreateFileA(file,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(h!=INVALID_HANDLE_VALUE){LARGE_INTEGER size={};
        if(GetFileSizeEx(h,&size)&&size.QuadPart>0&&size.QuadPart<16*1024*1024){data.resize(static_cast<size_t>(size.QuadPart));DWORD got=0;if(!ReadFile(h,&data[0],static_cast<DWORD>(data.size()),&got,nullptr))got=0;data.resize(got);}
        CloseHandle(h);}
    if(data.size()>=2&&static_cast<unsigned char>(data[0])==0xFF&&static_cast<unsigned char>(data[1])==0xFE){
        // A UTF-16 file: keep the text the ANSI profile API would have returned.
        std::wstring w((data.size()-2)/2,L'\0');memcpy(&w[0],data.data()+2,w.size()*2);
        int n=WideCharToMultiByte(CP_ACP,0,w.data(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);std::string a(n,'\0');
        if(n)WideCharToMultiByte(CP_ACP,0,w.data(),static_cast<int>(w.size()),&a[0],n,nullptr,nullptr);data=a;
    }else if(data.size()>=3&&!memcmp(data.data(),"\xEF\xBB\xBF",3)){bom=true;data.erase(0,3);}
    for(size_t start=0;start<data.size();){size_t end=data.find('\n',start);if(end==std::string::npos)end=data.size();
        std::string line=data.substr(start,end-start);if(!line.empty()&&line.back()=='\r')line.pop_back();lines.push_back(std::move(line));start=end+1;}
    reindex();readStamp();
}
static std::string text(){std::string out;if(bom)out="\xEF\xBB\xBF";for(const auto &line:lines){out+=line;out+="\r\n";}return out;}
// Written beside the INI and moved over it, so a crash mid-write never leaves a cut file.
static bool writeFile(const std::string &file,const std::string &content){
    auto put=[&](const std::string &target,DWORD creation){
        HANDLE h=CreateFileA(target.c_str(),GENERIC_WRITE,0,nullptr,creation,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
        DWORD written=0;bool ok=WriteFile(h,content.data(),static_cast<DWORD>(content.size()),&written,nullptr)&&written==content.size();CloseHandle(h);return ok;};
    std::string temp=file+".tmp";
    if(put(temp,CREATE_ALWAYS)&&MoveFileExA(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
    DeleteFileA(temp.c_str());
    return put(file,CREATE_ALWAYS);   // e.g. another program keeps the INI open without delete sharing
}
static bool flushLocked(){
    if(!loaded||!dirty)return true;
    if(!writeFile(path,text())){++failures;return false;}
    dirty=false;failures=0;readStamp();return true;
}
// Picks up edits made to the file by other programs while nothing is pending here.
// In game this looks at the file at most once a second (one look costs ~45 us).
static ULONGLONG stampChecked;
static void ensureLocked(const char *file){
    if(!loaded||_stricmp(path.c_str(),file)){if(loaded&&dirty)flushLocked();load(file);stampChecked=GetTickCount64();return;}
    if(dirty||writing)return;
    ULONGLONG tick=GetTickCount64();if(deferred&&tick-stampChecked<1000)return;stampChecked=tick;
    WIN32_FILE_ATTRIBUTE_DATA now;bool exists=GetFileAttributesExA(file,GetFileExInfoStandard,&now)!=0;
    if(!sameFile(now,exists,stamp,stampValid))load(file);
}
static void touched(){dirty=true;changedTick=GetTickCount64();if(!deferred)flushLocked();}
static bool find(const char *section,const char *key,std::string &value){
    auto found=index.find(lower(section)+'\n'+lower(key));if(found==index.end())return false;
    std::string name;entry(lines[found->second],name,value);
    if(value.size()>=2&&(value[0]=='"'||value[0]=='\'')&&value.back()==value[0])value=value.substr(1,value.size()-2);
    return true;
}
static void set(const char *section,const char *key,const char *value){
    auto found=index.find(lower(section)+'\n'+lower(key));
    if(found!=index.end()){std::string name,old;entry(lines[found->second],name,old);if(old==trim(value))return;
        lines[found->second]=name+"="+value;touched();return;}
    std::string wanted=lower(section);size_t at=lines.size();bool exists=false;
    for(size_t i=0;i<lines.size();++i){std::string name;if(!header(lines[i],name))continue;
        if(exists){at=i;break;}
        if(lower(name)==wanted){exists=true;at=lines.size();}}
    if(!exists){lines.push_back(std::string("[")+section+"]");lines.push_back(std::string(key)+"="+value);}
    else{while(at>0&&trim(lines[at-1]).empty())--at;lines.insert(lines.begin()+at,std::string(key)+"="+value);}
    reindex();touched();
}
static void removeKey(const char *section,const char *key){
    auto found=index.find(lower(section)+'\n'+lower(key));if(found==index.end())return;
    lines.erase(lines.begin()+found->second);reindex();touched();
}
static void removeSection(const char *section){
    std::string wanted=lower(section);
    for(size_t i=0;i<lines.size();++i){std::string name;if(!header(lines[i],name)||lower(name)!=wanted)continue;
        size_t end=i+1;while(end<lines.size()&&!header(lines[end],name))++end;
        lines.erase(lines.begin()+i,lines.begin()+end);reindex();touched();return;}
}
static long number(const char *s){
    // Same rules as the profile API (RtlCharToInteger, base 0).
    while(*s==' '||*s=='\t')++s;bool negative=false;if(*s=='+'||*s=='-'){negative=*s=='-';++s;}
    int base=10;if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')){base=16;s+=2;}else if(s[0]=='0'&&(s[1]=='o'||s[1]=='O')){base=8;s+=2;}else if(s[0]=='0'&&(s[1]=='b'||s[1]=='B')){base=2;s+=2;}
    unsigned long value=0;
    for(;;++s){int digit=*s>='0'&&*s<='9'?*s-'0':*s>='a'&&*s<='z'?*s-'a'+10:*s>='A'&&*s<='Z'?*s-'A'+10:99;if(digit>=base)break;value=value*base+digit;}
    return negative?-static_cast<long>(value):static_cast<long>(value);
}
static bool cached(const char *file){return file&&*file&&*iniPath&&!_stricmp(file,iniPath);}
// Synchronous write of pending changes (config profiles, unload).
static void flushNow(){
    BackgroundIo::drain();
    AcquireSRWLockExclusive(&lock);flushLocked();ReleaseSRWLockExclusive(&lock);
}
// The file was replaced on disk (config profile loaded): read it again on next use.
static void invalidate(){AcquireSRWLockExclusive(&lock);if(!dirty)loaded=false;ReleaseSRWLockExclusive(&lock);}
// Once per frame: starts the background write after values have been quiet for a moment.
static void tick(){
    if(!deferred)return;ULONGLONG now=GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    if(!loaded||!dirty||writing||now-changedTick<(failures?5000:quietMs)){ReleaseSRWLockExclusive(&lock);return;}
    std::string file=path,content=text();writing=true;dirty=false;
    ReleaseSRWLockExclusive(&lock);
    BackgroundIo::post([file,content]{
        bool ok=writeFile(file,content);
        AcquireSRWLockExclusive(&lock);writing=false;
        if(ok){failures=0;if(!_stricmp(file.c_str(),path.c_str()))readStamp();}
        else{dirty=true;changedTick=GetTickCount64();if(++failures==1)logLine("Settings: CS16Customizer.ini could not be written; retrying.");}
        ReleaseSRWLockExclusive(&lock);
    });
}
// Process exit: other threads are gone, so write directly if nothing holds the lock.
static void emergencyFlush(){
    if(!TryAcquireSRWLockExclusive(&lock))return;
    if(loaded&&(dirty||writing)&&writeFile(path,text()))dirty=false;
    ReleaseSRWLockExclusive(&lock);
}

static DWORD WINAPI getString(LPCSTR section,LPCSTR key,LPCSTR fallback,LPSTR out,DWORD size,LPCSTR file){
    if(!cached(file)||!section||!key){if(cached(file))flushNow();return ::GetPrivateProfileStringA(section,key,fallback,out,size,file);}
    if(!out||!size)return 0;
    AcquireSRWLockExclusive(&lock);ensureLocked(file);std::string value;bool found=find(section,key,value);ReleaseSRWLockExclusive(&lock);
    if(!found){value=fallback?fallback:"";size_t last=value.find_last_not_of(" \t");value.resize(last==std::string::npos?0:last+1);}
    size_t n=std::min<size_t>(value.size(),size-1);memcpy(out,value.data(),n);out[n]=0;return static_cast<DWORD>(n);
}
static UINT WINAPI getInt(LPCSTR section,LPCSTR key,INT fallback,LPCSTR file){
    if(!cached(file)||!section||!key)return ::GetPrivateProfileIntA(section,key,fallback,file);
    char value[64];if(!getString(section,key,"",value,sizeof(value),file))return static_cast<UINT>(fallback);
    return static_cast<UINT>(number(value));
}
static BOOL WINAPI writeString(LPCSTR section,LPCSTR key,LPCSTR value,LPCSTR file){
    if(!cached(file))return ::WritePrivateProfileStringA(section,key,value,file);
    if(!section&&!key&&!value){flushNow();return TRUE;}     // "flush the cache" request
    if(!section){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    AcquireSRWLockExclusive(&lock);ensureLocked(file);
    if(!key)removeSection(section);else if(!value)removeKey(section,key);else set(section,key,value);
    bool ok=deferred||!dirty;ReleaseSRWLockExclusive(&lock);return ok;
}
static DWORD WINAPI getSection(LPCSTR section,LPSTR out,DWORD size,LPCSTR file){
    if(cached(file))flushNow();return ::GetPrivateProfileSectionA(section,out,size,file);
}
static BOOL WINAPI writeSection(LPCSTR section,LPCSTR values,LPCSTR file){
    if(!cached(file))return ::WritePrivateProfileSectionA(section,values,file);
    flushNow();BOOL ok=::WritePrivateProfileSectionA(section,values,file);invalidate();return ok;
}
}
#define GetPrivateProfileStringA IniCache::getString
#define GetPrivateProfileIntA IniCache::getInt
#define WritePrivateProfileStringA IniCache::writeString
#define GetPrivateProfileSectionA IniCache::getSection
#define WritePrivateProfileSectionA IniCache::writeSection
