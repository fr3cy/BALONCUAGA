namespace RadarTextures {
struct Image {int width=0,height=0;std::vector<byte> rgb,alpha;std::string name;};   // alpha: only '{' textures (fences, grates)
struct Mip {char name[16];uint32_t width,height,offset[4];};
static std::string key(const char *name){std::string result(name,strnlen(name,16));for(char &c:result)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return result;}
static bool decode(const byte *data,size_t size,Image &out){
    if(size<sizeof(Mip))return false;Mip mip;memcpy(&mip,data,sizeof(mip));
    if(!mip.width||!mip.height||mip.width>4096||mip.height>4096||mip.width%8||mip.height%8||!mip.offset[2]||!mip.offset[3])return false;
    size_t pixels=static_cast<size_t>(mip.width)*mip.height,palette=static_cast<size_t>(mip.offset[3])+(mip.width/8)*(mip.height/8);
    if(mip.offset[0]<sizeof(Mip)||mip.offset[0]>size||pixels>size-mip.offset[0]||palette>size||size-palette<2)return false;
    uint16_t colors;memcpy(&colors,data+palette,2);palette+=2;if(colors!=256||size-palette<768)return false;
    out.width=mip.width;out.height=mip.height;out.rgb.resize(pixels*3);
    for(size_t i=0;i<pixels;++i)memcpy(out.rgb.data()+i*3,data+palette+data[mip.offset[0]+i]*3,3);
    // '{' textures draw palette index 255 as see-through (fences, grates, foliage).
    out.alpha.clear();if(mip.name[0]=='{'){out.alpha.resize(pixels);for(size_t i=0;i<pixels;++i)out.alpha[i]=data[mip.offset[0]+i]==255?0:255;}
    return true;
}
static std::vector<byte> readWad(const std::string &name){
    auto bytes=Preview::read(name.c_str());if(!bytes.empty())return bytes;
    std::string path=std::string(rootPath)+"\\valve\\"+name;FILE *f=fopen(path.c_str(),"rb");if(!f)return {};
    fseek(f,0,SEEK_END);long length=ftell(f);rewind(f);if(length>0&&length<=64*1024*1024){bytes.resize(length);if(fread(bytes.data(),1,length,f)!=static_cast<size_t>(length))bytes.clear();}fclose(f);return bytes;
}
struct WadEntry {int32_t offset,diskSize,size;byte type,compressed,pad[2];char name[16];};
static std::vector<Image> load(const std::vector<byte> &bsp,int textureOffset,int textureLength,int entityOffset,int entityLength){
    if(textureOffset<0||textureLength<4||static_cast<size_t>(textureOffset)>bsp.size()||static_cast<size_t>(textureLength)>bsp.size()-textureOffset)return {};
    const byte *textures=bsp.data()+textureOffset;int32_t count;memcpy(&count,textures,4);if(count<1||count>4096||4+static_cast<size_t>(count)*4>static_cast<size_t>(textureLength))return {};
    std::vector<Image> result(count);std::vector<std::string> names(count);size_t total=0;
    for(int i=0;i<count;++i){int32_t offset;memcpy(&offset,textures+4+i*4,4);if(offset<0||static_cast<size_t>(offset)+sizeof(Mip)>static_cast<size_t>(textureLength))continue;
        names[i]=key(reinterpret_cast<const char *>(textures+offset));if(decode(textures+offset,textureLength-offset,result[i]))total+=result[i].rgb.size();if(total>128*1024*1024)return {};
    }
    std::vector<std::string> wads;
    if(entityOffset>=0&&entityLength>0&&static_cast<size_t>(entityOffset)<bsp.size()&&static_cast<size_t>(entityLength)<=bsp.size()-entityOffset){
        std::string entities(reinterpret_cast<const char *>(bsp.data()+entityOffset),entityLength);auto at=entities.find("\"wad\"");
        if(at!=std::string::npos){auto begin=entities.find('"',at+5),end=begin==std::string::npos?begin:entities.find('"',begin+1);
            if(end!=std::string::npos){std::istringstream list(entities.substr(begin+1,end-begin-1));std::string item;while(std::getline(list,item,';')&&wads.size()<24){
                auto slash=item.find_last_of("/\\");if(slash!=std::string::npos)item.erase(0,slash+1);
                if(item.size()>4&&item.size()<64&&item.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ")==std::string::npos&&item.find("..")==std::string::npos)wads.push_back(item);
            }}
        }
    }
    for(const char *name:{"cstrike.wad","halflife.wad"})if(std::find(wads.begin(),wads.end(),name)==wads.end())wads.push_back(name);
    for(const auto &name:wads){auto wad=readWad(name);if(wad.size()<12||memcmp(wad.data(),"WAD3",4))continue;int32_t entries,offset;memcpy(&entries,wad.data()+4,4);memcpy(&offset,wad.data()+8,4);
        if(entries<0||entries>65536||offset<0||static_cast<size_t>(offset)>wad.size()||static_cast<size_t>(entries)*sizeof(WadEntry)>wad.size()-offset)continue;
        for(int j=0;j<entries;++j){WadEntry entry;memcpy(&entry,wad.data()+offset+j*sizeof(entry),sizeof(entry));
            if(entry.compressed||entry.type!=67||entry.offset<0||entry.diskSize<40||static_cast<size_t>(entry.offset)>wad.size()||static_cast<size_t>(entry.diskSize)>wad.size()-entry.offset)continue;
            auto name=key(entry.name);for(int i=0;i<count;++i)if(result[i].rgb.empty() && names[i]==name){
                if(decode(wad.data()+entry.offset,entry.diskSize,result[i]))total+=result[i].rgb.size();if(total>128*1024*1024)return {};
            }
        }
    }for(int i=0;i<count;++i)result[i].name=names[i];return result;
}
}
