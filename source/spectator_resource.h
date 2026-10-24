#pragma once
// Independently authored transform of the user's existing resource, not a bundled mod.
// Only the three background controls change. Other widgets retain their original bytes.
namespace SpectatorResource {
static bool enabled=false;
static bool restartRequired=false;
static const char *state=""; // language-independent result code; UI supplies translations
struct Token {std::string text;size_t first,last;};
static bool tokenize(const std::string &s,std::vector<Token> &out){
    size_t i=s.size()>=3&&!memcmp(s.data(),"\xef\xbb\xbf",3)?3:0;
    while(i<s.size()){
        if(static_cast<unsigned char>(s[i])<=32){++i;continue;}
        if(i+1<s.size()&&s[i]=='/'&&s[i+1]=='/'){while(i<s.size()&&s[i]!='\n')++i;continue;}
        if(i+1<s.size()&&s[i]=='/'&&s[i+1]=='*'){size_t end=s.find("*/",i+2);if(end==std::string::npos)return false;i=end+2;continue;}
        size_t start=i;std::string value;
        if(s[i]=='"'){++i;bool closed=false;while(i<s.size()){if(s[i]=='"'){++i;closed=true;break;}if(s[i]=='\\'&&i+1<s.size()&&(s[i+1]=='"'||s[i+1]=='\\'))++i;value+=s[i++];}if(!closed)return false;}
        else if(s[i]=='{'||s[i]=='}')value+=s[i++];
        else{while(i<s.size()&&static_cast<unsigned char>(s[i])>32&&s[i]!='{'&&s[i]!='}')value+=s[i++];}
        out.push_back({value,start,i});if(out.size()>100000)return false;
    }return true;
}
struct Edit {size_t first,last;std::string value;};
static bool transform(const std::string &source,std::string &result){
    std::vector<Token> t;if(!tokenize(source,t)||t.size()<4)return false;
    std::vector<Edit> edits;int depth=0,found=0;bool root=false;
    for(size_t i=0;i<t.size();++i){
        if(t[i].text=="{"){++depth;continue;}if(t[i].text=="}"){if(--depth<0)return false;continue;}
        if(i==0)root=!_stricmp(t[i].text.c_str(),"Resource/UI/SpectatorGUI.res");
        if(depth!=1||i+1>=t.size()||t[i+1].text!="{")continue;
        int bit=!_stricmp(t[i].text.c_str(),"TopBar")?1:!_stricmp(t[i].text.c_str(),"BottomBar")?2:!_stricmp(t[i].text.c_str(),"bottombarblank")?4:0;
        if(!bit)continue;if(found&bit)return false;found|=bit;
        size_t end=i+2;for(;end<t.size()&&t[end].text!="}";++end)if(t[end].text=="{")return false;if(end>=t.size())return false;
        const char *keys[]={"ControlName","tall","visible","paintbackground","WizardWide","WizardTall"};
        const char *values[]={"WizardSubPanel","0","0","0","0","0"};
        for(int key=0;key<6;++key){bool present=false;for(size_t j=i+2;j<end;j+=2){if(j+1>=end)return false;
            if(!_stricmp(t[j].text.c_str(),keys[key])){edits.push_back({t[j+1].first,t[j+1].last,std::string("\"")+values[key]+"\""});present=true;}}
            if(!present)edits.push_back({t[end].first,t[end].first,std::string("\t\t\"")+keys[key]+"\" \""+values[key]+"\"\r\n"});
        }
    }
    if(!root||depth!=0||found!=7)return false;
    std::stable_sort(edits.begin(),edits.end(),[](const Edit &a,const Edit &b){return a.first>b.first;});
    result=source;for(const auto &e:edits)result.replace(e.first,e.last-e.first,e.value);return true;
}
static bool read(const std::string &path,std::string &out){
    HANDLE file=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    DWORD size=GetFileSize(file,nullptr),got=0;bool ok=size>0&&size<=1024*1024;
    if(ok){out.resize(size);ok=ReadFile(file,out.data(),size,&got,nullptr)&&got==size;}CloseHandle(file);return ok;
}
static bool exists(const std::string &path){return GetFileAttributesA(path.c_str())!=INVALID_FILE_ATTRIBUTES;}
static bool create(const std::string &path,const std::string &bytes){
    HANDLE file=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    DWORD put=0;bool ok=WriteFile(file,bytes.data(),bytes.size(),&put,nullptr)&&put==bytes.size();if(ok)ok=FlushFileBuffers(file);CloseHandle(file);if(!ok)DeleteFileA(path.c_str());return ok;
}
static bool replace(const std::string &path,const std::string &bytes){
    std::string temp=path+".baloncuaga-v72.tmp";if(!create(temp,bytes))return false;
    bool ok=MoveFileExA(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;if(!ok)DeleteFileA(temp.c_str());return ok;
}
static bool apply(const std::string &path,bool wanted){
    std::string backupPath=path+".baloncuaga-v72.original",managedPath=path+".baloncuaga-v72.patched";
    std::string current,original,patched;bool hasOriginal=exists(backupPath),hasPatched=exists(managedPath);
    if(!wanted&&!hasOriginal&&!hasPatched){enabled=false;state="off";return true;}
    if(!read(path,current)){state="read";return false;}
    if(hasOriginal){
        if(!read(backupPath,original)||!transform(original,patched)){state="backup";return false;}
        if(hasPatched){std::string saved;if(!read(managedPath,saved)||saved!=patched){state="backup";return false;}}
        else if(current!=original||!create(managedPath,patched)){state="backup";return false;}
        if(current!=original&&current!=patched){state="conflict";return false;}
    }else{
        if(hasPatched){state="backup";return false;}
        original=current;if(!transform(original,patched)){state="format";return false;}
        if(wanted&&(!create(backupPath,original)||!create(managedPath,patched))){state="write";return false;}
    }
    const auto &target=wanted?patched:original;
    if(current!=target){if(!replace(path,target)){state="write";return false;}restartRequired=true;}
    if(!wanted){DeleteFileA(managedPath.c_str());DeleteFileA(backupPath.c_str());}
    enabled=wanted;state=restartRequired?"restart":wanted?"on":"off";return true;
}
static std::string path(){return std::string(rootPath)+"/cstrike/resource/UI/Spectator.res";}
static bool setEnabled(bool wanted){return SpectatorResource::apply(path(),wanted);}
static void save(){WritePrivateProfileStringA("interface","spectator_no_bars",enabled?"1":"0",iniPath);}
static void load(){bool wanted=GetPrivateProfileIntA("interface","spectator_no_bars",0,iniPath)!=0;setEnabled(wanted);}
}
