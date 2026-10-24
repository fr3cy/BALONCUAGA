#include "radar_textures.h"
// GoldSrc BSP v30 world geometry fallback (Valve SDK utils/common/bspfile.h).
namespace BspRadar {
struct Lump {int32_t offset,length;};
struct Header {int32_t version;Lump lumps[15];};
struct Vertex {float p[3];};
struct Plane {float normal[3],distance;int32_t type;};
struct Edge {uint16_t v[2];};
struct Face {uint16_t plane;int16_t side;int32_t first;int16_t count,texture;byte styles[4];int32_t light;};
struct TexInfo {float vectors[2][4];int32_t texture,flags;};
struct World {float mins[3],maxs[3],origin[3];int32_t head[4],leaves,first,count;};
template<class T>static const T *lump(const std::vector<byte> &data,const Header &header,int index,size_t &count){
    auto part=header.lumps[index];count=0;
    if(part.offset<0 || part.length<0 || part.length%sizeof(T) || static_cast<size_t>(part.offset)>data.size() || static_cast<size_t>(part.length)>data.size()-part.offset)return nullptr;
    count=part.length/sizeof(T);return count?reinterpret_cast<const T *>(data.data()+part.offset):nullptr;
}
// A face range to rasterise: the world (owner 0) or one visible brush entity
// such as a crate (func_wall / func_breakable / func_illusionary / doors).
struct DrawModel {int first,count;float offset[3];uint16_t owner;};
static bool hiddenClass(const std::string &name){
    static const char *prefixes[]={"trigger_","func_buyzone","func_bomb_target","func_hostage_rescue","func_vip_safetyzone","func_escapezone",
        "func_ladder","func_monsterclip","func_mortar_field","func_traincontrols","func_weaponcheck","env_bubbles","game_zone_player","info_"};
    for(const char *prefix:prefixes)if(name.rfind(prefix,0)==0)return true;
    return false;
}
// Compile-tool textures that are invisible in game.
static bool toolTexture(const std::string &name){
    for(const char *tool:{"aaatrigger","clip","null","origin","sky","hint","skip","bevel","clipbevel","nodraw"})if(name==tool)return true;
    return false;
}
static bool hiddenFace(const std::vector<RadarTextures::Image> &materials,const TexInfo &mapping){
    if(mapping.flags&1)return true; // sky
    return mapping.texture>=0&&static_cast<size_t>(mapping.texture)<materials.size()&&toolTexture(materials[mapping.texture].name);
}
// One entity from the BSP entity lump, with the keys the radar needs.
struct Entity {std::string classname,model;float origin[3]={0,0,0};float yaw=0;bool invisible=false;};
static bool readVector(const std::string &text,float out[3]){
    float v[3];if(sscanf(text.c_str(),"%f %f %f",&v[0],&v[1],&v[2])!=3)return false;
    for(float c:v)if(!std::isfinite(c)||std::abs(c)>=65536)return false;
    for(int i=0;i<3;++i)out[i]=v[i];return true;
}
static std::vector<Entity> parseEntities(const std::vector<byte> &data,const Header &header){
    std::vector<Entity> result;auto part=header.lumps[0];
    if(part.offset<0||part.length<=0||static_cast<size_t>(part.offset)>data.size()||static_cast<size_t>(part.length)>data.size()-part.offset)return result;
    const char *base=reinterpret_cast<const char *>(data.data()+part.offset);std::string text(base,strnlen(base,part.length));
    size_t at=0;
    while((at=text.find('{',at))!=std::string::npos&&result.size()<8192){
        size_t end=text.find('}',at);if(end==std::string::npos)break;
        Entity e;std::string rendermode,renderamt;
        for(size_t p=at+1;;){
            size_t k0=text.find('"',p);if(k0==std::string::npos||k0>end)break;size_t k1=text.find('"',k0+1);if(k1==std::string::npos||k1>end)break;
            size_t v0=text.find('"',k1+1);if(v0==std::string::npos||v0>end)break;size_t v1=text.find('"',v0+1);if(v1==std::string::npos||v1>end)break;
            std::string key=text.substr(k0+1,k1-k0-1),value=text.substr(v0+1,v1-v0-1);p=v1+1;
            if(key=="classname")e.classname=value;else if(key=="model")e.model=value;else if(key=="origin")readVector(value,e.origin);
            else if(key=="angles"){float a[3];if(readVector(value,a))e.yaw=a[1];}else if(key=="angle"){float a=static_cast<float>(atof(value.c_str()));if(std::isfinite(a)&&a>=0)e.yaw=a;}
            else if(key=="rendermode")rendermode=value;else if(key=="renderamt")renderamt=value;
        }
        at=end+1;
        for(char &c:e.classname)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        e.invisible=!rendermode.empty()&&atoi(rendermode.c_str())!=0&&!renderamt.empty()&&atoi(renderamt.c_str())==0;
        result.push_back(std::move(e));
    }
    return result;
}
// World geometry plus every visible brush entity, each shifted by its "origin"
// key exactly like the engine places it. Works the same for every map.
static std::vector<DrawModel> drawModels(const std::vector<Entity> &entities,const World *models,size_t modelCount,size_t faceCount,int &owner){
    std::vector<DrawModel> parts;parts.push_back({models[0].first,models[0].count,{0,0,0},0});
    for(const auto &e:entities){
        if(parts.size()>=4096)break;
        if(e.model.size()<2||e.model[0]!='*'||e.invisible||hiddenClass(e.classname))continue;
        int index=atoi(e.model.c_str()+1);if(index<=0||static_cast<size_t>(index)>=modelCount)continue;
        const auto &m=models[index];if(m.first<0||m.count<=0||static_cast<size_t>(m.first)+m.count>faceCount)continue;
        parts.push_back({m.first,m.count,{e.origin[0],e.origin[1],e.origin[2]},static_cast<uint16_t>(std::min(++owner,65535))});
    }
    return parts;
}
// Studio-model props placed on the map (trees, barrels, cars, decorations):
// their footprint from the model's bounding box, rotated by the entity yaw.
struct Prop {float corner[4][2];float top,bottom;uint16_t owner;};
static bool hiddenProp(const std::string &name){
    for(const char *prefix:{"hostage_entity","monster_scientist","monster_barney","player","weapon_","ammo_"})if(name.rfind(prefix,0)==0)return true;
    return false;
}
static std::vector<Prop> props(const std::vector<Entity> &entities,int &owner){
    std::vector<Prop> result;struct Box{bool ok;float v[6];};std::unordered_map<std::string,Box> boxes;
    for(const auto &e:entities){
        if(result.size()>=4096)break;
        if(e.invisible||hiddenClass(e.classname)||hiddenProp(e.classname)||e.model.size()<5)continue;
        std::string path=e.model;for(char &c:path)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));std::replace(path.begin(),path.end(),'\\','/');
        if(path.compare(path.size()-4,4,".mdl")||path.find("..")!=std::string::npos)continue;
        auto found=boxes.find(path);
        if(found==boxes.end()){
            Box box={false,{0,0,0,0,0,0}};auto bytes=Preview::read(path.c_str());
            if(bytes.size()>=sizeof(studiohdr_t)){studiohdr_t h;memcpy(&h,bytes.data(),sizeof(h));
                const float *lo=h.bbmin,*hi=h.bbmax;if(!(hi[0]>lo[0]&&hi[1]>lo[1])){lo=h.min;hi=h.max;}
                box.ok=hi[0]>lo[0]&&hi[1]>lo[1]&&hi[0]-lo[0]<4096&&hi[1]-lo[1]<4096&&std::isfinite(hi[2]);
                if(box.ok){box.v[0]=lo[0];box.v[1]=lo[1];box.v[2]=lo[2];box.v[3]=hi[0];box.v[4]=hi[1];box.v[5]=hi[2];}
            }
            found=boxes.emplace(path,box).first;
        }
        if(!found->second.ok)continue;const float *b=found->second.v;
        float s=std::sin(e.yaw*.017453293f),c=std::cos(e.yaw*.017453293f);Prop p;
        const float xs[4]={b[0],b[3],b[3],b[0]},ys[4]={b[1],b[1],b[4],b[4]};
        for(int i=0;i<4;++i){p.corner[i][0]=e.origin[0]+xs[i]*c-ys[i]*s;p.corner[i][1]=e.origin[1]+xs[i]*s+ys[i]*c;}
        p.top=e.origin[2]+b[5];p.bottom=e.origin[2]+b[2];p.owner=static_cast<uint16_t>(std::min(++owner,65535));result.push_back(p);
    }
    return result;
}
// Set by the render thread when the map changes while the builder thread still works.
static std::atomic<bool> cancel{false};
// The parsed map, kept while it is played: when the player walks under a roof the
// radar rasterises the floor below the roof again from it.
struct Scene {
    std::vector<byte> data;Header header{};
    const Vertex *vertices=nullptr;const Edge *edges=nullptr;const int32_t *surf=nullptr;const Face *faces=nullptr;const Plane *planes=nullptr;const TexInfo *textures=nullptr;const World *models=nullptr;
    size_t nv=0,ne=0,ns=0,nf=0,np=0,nt=0,nm=0;
    std::vector<RadarTextures::Image> materials;std::vector<DrawModel> parts;std::vector<Prop> props;
    float minX=0,maxX=0,minY=0,maxY=0,floorZ=0;
};
static std::shared_ptr<Scene> loadScene(std::vector<byte> bytes){
    auto scene=std::make_shared<Scene>();auto &s=*scene;s.data=std::move(bytes);const auto &data=s.data;
    if(data.size()<sizeof(Header))return nullptr;
    memcpy(&s.header,data.data(),sizeof(s.header));if(s.header.version!=30)return nullptr;
    s.vertices=lump<Vertex>(data,s.header,3,s.nv);s.edges=lump<Edge>(data,s.header,12,s.ne);
    s.surf=lump<int32_t>(data,s.header,13,s.ns);s.faces=lump<Face>(data,s.header,7,s.nf);
    s.planes=lump<Plane>(data,s.header,1,s.np);s.textures=lump<TexInfo>(data,s.header,6,s.nt);s.models=lump<World>(data,s.header,14,s.nm);
    if(!s.vertices||!s.edges||!s.surf||!s.faces||!s.planes||!s.textures||!s.models || s.nv>65536 || s.nf>65536 || s.ne>1000000 || s.ns>2000000)return nullptr;
    const auto &world=s.models[0];s.minX=world.mins[0]-32;s.maxX=world.maxs[0]+32;s.minY=world.mins[1]-32;s.maxY=world.maxs[1]+32;s.floorZ=world.mins[2];
    if(!std::isfinite(s.minX)||!std::isfinite(s.maxX)||!std::isfinite(s.minY)||!std::isfinite(s.maxY)||!std::isfinite(world.mins[2])||std::abs(world.mins[2])>65536||s.maxX-s.minX<1||s.maxY-s.minY<1||s.maxX-s.minX>65536||s.maxY-s.minY>65536 || world.first<0||world.count<0||static_cast<size_t>(world.first)+world.count>s.nf)return nullptr;
    s.materials=RadarTextures::load(data,s.header.lumps[2].offset,s.header.lumps[2].length,s.header.lumps[0].offset,s.header.lumps[0].length);
    int textured=0;for(const auto &m:s.materials)if(!m.rgb.empty())++textured;
    {char message[120];snprintf(message,sizeof(message),"Radar BSP textures: %d / %u decoded from BSP/WAD.",textured,static_cast<unsigned>(s.materials.size()));logLine(message);}
    auto entities=parseEntities(data,s.header);int owner=0;s.parts=drawModels(entities,s.models,s.nm,s.nf,owner);s.props=props(entities,owner);
    {char message[128];snprintf(message,sizeof(message),"Radar BSP: world + %u brush objects (crates, walls, doors) + %u model props.",static_cast<unsigned>(s.parts.size()-1),static_cast<unsigned>(s.props.size()));logLine(message);}
    return scene;
}
// Pixel grid: the whole-map grid has spanX/w1 world units per pixel; a window on it
// starts at pixel (ox,oy) of that grid, so its pixels line up with the map's.
struct Grid {float minX,maxY,spanX,spanY,w1,h1,ox,oy;int width,height;};
struct Raster {std::vector<byte> surface;std::vector<float> z,top;std::vector<uint16_t> owners;size_t painted=0;};
// Rasterises every visible upward face and model prop. The colour, height and owner
// buffers keep the highest surface not above `cutoff`; `top` (when asked) keeps the
// highest surface of all, so a roof over the player can be recognised.
static bool rasterize(const Scene &s,const Grid &g,float cutoff,bool withTop,Raster &r,const std::atomic<bool> *abandon=nullptr){
    const int width=g.width,height=g.height;const size_t pixels=static_cast<size_t>(width)*height;
    r.surface.assign(pixels*3,0);r.z.assign(pixels,-1e30f);r.owners.assign(pixels,0);if(withTop)r.top.assign(pixels,-1e30f);else r.top.clear();r.painted=0;
    size_t budget=0;const float unitsPerPixel=g.spanX/g.w1;
    struct Point {float x,y,z;};
    auto cross=[](const Point &a,const Point &b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
    for(const auto &part:s.parts)for(int fi=part.first;fi<part.first+part.count;++fi){const auto &face=s.faces[fi];if(cancel.load(std::memory_order_relaxed)||(abandon&&abandon->load(std::memory_order_relaxed)))return false;
        if(face.plane>=s.np||face.texture<0||static_cast<size_t>(face.texture)>=s.nt||face.first<0||face.count<3||face.count>1024||static_cast<size_t>(face.first)+face.count>s.ns)continue;
        float normal=s.planes[face.plane].normal[2]*(face.side?-1:1);if(!std::isfinite(normal)||normal<.25f || hiddenFace(s.materials,s.textures[face.texture]))continue;
        const auto &mapping=s.textures[face.texture];
        const RadarTextures::Image *material=mapping.texture>=0&&static_cast<size_t>(mapping.texture)<s.materials.size()&&!s.materials[mapping.texture].rgb.empty()?&s.materials[mapping.texture]:nullptr;
        std::vector<Point> polygon;bool valid=true;float lowX=1e30f,highX=-1e30f,lowY=1e30f,highY=-1e30f;
        for(int i=0;i<face.count;++i){int64_t edge=s.surf[face.first+i];uint64_t index=edge<0?-edge:edge;
            if(index>=s.ne){valid=false;break;}int vertex=s.edges[index].v[edge<0?1:0];if(static_cast<size_t>(vertex)>=s.nv){valid=false;break;}
            const auto &v=s.vertices[vertex];if(!std::isfinite(v.p[0])||!std::isfinite(v.p[1])||!std::isfinite(v.p[2])||std::abs(v.p[0])>65536||std::abs(v.p[1])>65536||std::abs(v.p[2])>65536){valid=false;break;}
            float vx=v.p[0]+part.offset[0],vy=v.p[1]+part.offset[1],vz=v.p[2]+part.offset[2];
            Point p={(vx-g.minX)/g.spanX*g.w1-g.ox,(g.maxY-vy)/g.spanY*g.h1-g.oy,vz};polygon.push_back(p);
            lowX=std::min(lowX,p.x);highX=std::max(highX,p.x);lowY=std::min(lowY,p.y);highY=std::max(highY,p.y);
        }if(!valid)continue;
        if(highX<-2||lowX>width+2||highY<-2||lowY>height+2)continue;   // outside this window
        for(size_t i=1;i+1<polygon.size();++i){auto a=polygon[0],b=polygon[i],c=polygon[i+1];float area=cross(a,b,c.x,c.y);if(std::abs(area)<.001f)continue;
            int left=std::max(0,static_cast<int>(std::floor(std::min({a.x,b.x,c.x})))),right=std::min(width-1,static_cast<int>(std::ceil(std::max({a.x,b.x,c.x}))));
            int top=std::max(0,static_cast<int>(std::floor(std::min({a.y,b.y,c.y})))),bottom=std::min(height-1,static_cast<int>(std::ceil(std::max({a.y,b.y,c.y}))));
            if(left>right||top>bottom)continue;budget+=static_cast<size_t>(right-left+1)*(bottom-top+1);if(budget>1500000000)return false;
            for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){float u=cross(b,c,x+.5f,y+.5f)/area,v=cross(c,a,x+.5f,y+.5f)/area,w=1-u-v;
                if(u<-.001f||v<-.001f||w<-.001f)continue;float value=u*a.z+v*b.z+w*c.z;size_t at=static_cast<size_t>(y)*width+x;
                if(withTop&&value>r.top[at])r.top[at]=value;
                if(value>cutoff)continue;
                float &current=r.z[at];if(value>current){if(current<-1e29f)++r.painted;current=value;r.owners[at]=part.owner;
                    if(material){float wx=g.minX+(x+g.ox+.5f)/g.w1*g.spanX-part.offset[0],wy=g.maxY-(y+g.oy+.5f)/g.h1*g.spanY-part.offset[1],wz=value-part.offset[2];
                        float tx=wx*mapping.vectors[0][0]+wy*mapping.vectors[0][1]+wz*mapping.vectors[0][2]+mapping.vectors[0][3];
                        float ty=wx*mapping.vectors[1][0]+wy*mapping.vectors[1][1]+wz*mapping.vectors[1][2]+mapping.vectors[1][3];
                        if(std::isfinite(tx)&&std::isfinite(ty)&&std::abs(tx)<1e9f&&std::abs(ty)<1e9f){
                            // Average the texels under this radar pixel (box filter) so
                            // small bright texels do not turn into sparkling specks.
                            float stepX=unitsPerPixel*std::hypot(mapping.vectors[0][0],mapping.vectors[0][1]),stepY=unitsPerPixel*std::hypot(mapping.vectors[1][0],mapping.vectors[1][1]);
                            int k=std::clamp(static_cast<int>(std::ceil(std::max(stepX,stepY))),1,4);int sum[3]={0,0,0};
                            for(int j=0;j<k;++j)for(int i=0;i<k;++i){
                                int sx=static_cast<int>(std::floor(tx+(i+.5f)*stepX/k-stepX*.5f))%material->width,sy=static_cast<int>(std::floor(ty+(j+.5f)*stepY/k-stepY*.5f))%material->height;
                                if(sx<0)sx+=material->width;if(sy<0)sy+=material->height;
                                const byte *t=material->rgb.data()+(static_cast<size_t>(sy)*material->width+sx)*3;sum[0]+=t[0];sum[1]+=t[1];sum[2]+=t[2];
                            }
                            byte *dst=r.surface.data()+at*3;for(int c=0;c<3;++c)dst[c]=static_cast<byte>(sum[c]/(k*k));
                        }
                    }else {r.surface[at*3]=75;r.surface[at*3+1]=94;r.surface[at*3+2]=102;}
                }
            }
        }
    }
    // Model props: their footprint is filled at the model's top height in a warm
    // neutral tone; the owner change around it produces the white outline.
    for(const auto &prop:s.props){
        Point q[4];for(int i=0;i<4;++i)q[i]={(prop.corner[i][0]-g.minX)/g.spanX*g.w1-g.ox,(g.maxY-prop.corner[i][1])/g.spanY*g.h1-g.oy,prop.top};
        for(int tri=0;tri<2;++tri){const Point &a=q[0],&b=q[tri+1],&c=q[tri+2];float area=cross(a,b,c.x,c.y);if(std::abs(area)<.001f)continue;
            int left=std::max(0,static_cast<int>(std::floor(std::min({a.x,b.x,c.x})))),right=std::min(width-1,static_cast<int>(std::ceil(std::max({a.x,b.x,c.x}))));
            int top=std::max(0,static_cast<int>(std::floor(std::min({a.y,b.y,c.y})))),bottom=std::min(height-1,static_cast<int>(std::ceil(std::max({a.y,b.y,c.y}))));
            if(left>right||top>bottom)continue;budget+=static_cast<size_t>(right-left+1)*(bottom-top+1);if(budget>1500000000)return false;
            for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){float u=cross(b,c,x+.5f,y+.5f)/area,v=cross(c,a,x+.5f,y+.5f)/area,w=1-u-v;
                if(u<-.001f||v<-.001f||w<-.001f)continue;size_t at=static_cast<size_t>(y)*width+x;
                if(withTop&&prop.top>r.top[at])r.top[at]=prop.top;
                if(prop.top>cutoff)continue;
                float &current=r.z[at];if(prop.top<=current)continue;
                if(current<-1e29f)++r.painted;current=prop.top;r.owners[at]=prop.owner;byte *dst=r.surface.data()+at*3;dst[0]=122;dst[1]=112;dst[2]=94;}
        }
    }
    return true;
}
// `scene` (optional) keeps the parsed map; `heights` (optional) receives the top
// surface height per map pixel, -32768 where nothing is drawn.
static bool generate(const std::vector<byte> &data,std::vector<byte> &rgba,int &width,int &height,float &minX,float &maxX,float &minY,float &maxY,
                     std::shared_ptr<Scene> *scene=nullptr,std::vector<int16_t> *heights=nullptr){
    auto parsed=loadScene(data);if(!parsed)return false;const auto &s=*parsed;
    minX=s.minX;maxX=s.maxX;minY=s.minY;maxY=s.maxY;
    width=height=2048;Grid grid={minX,maxY,maxX-minX,maxY-minY,static_cast<float>(width-1),static_cast<float>(height-1),0,0,width,height};
    Raster r;if(!rasterize(s,grid,FLT_MAX,false,r))return false;
    if(r.painted<100)return false;
    const auto &z=r.z;const auto &surface=r.surface;const auto &owners=r.owners;
    rgba.resize(width*height*4);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){int p=y*width+x;float value=z[p];int red=19,green=25,blue=29;
        if(value>-1e29f){red=surface[p*3];green=surface[p*3+1];blue=surface[p*3+2];
            bool boundary=x==0||y==0||x==width-1||y==height-1;
            // White outline at height steps and around every object, even a low one
            // standing flush with the floor or touching another object.
            if(!boundary)for(int d:{-1,1,-width,width}){int other=p+d;
                if(z[other]<-1e29f||std::abs(z[other]-value)>32||(owners[other]!=owners[p]&&(owners[other]||owners[p]))){boundary=true;break;}}
            if(boundary){red=247;green=246;blue=235;}
        }rgba[p*4]=red;rgba[p*4+1]=green;rgba[p*4+2]=blue;rgba[p*4+3]=value>-1e29f?255:0;
    }
    if(heights){heights->resize(z.size());for(size_t p=0;p<z.size();++p)(*heights)[p]=z[p]>-1e29f?static_cast<int16_t>(std::clamp(std::lround(z[p]),-32767L,32767L)):static_cast<int16_t>(-32768);}
    if(scene)*scene=parsed;
    return true;
}
static bool generate(const std::string &map,std::vector<byte> &rgba,int &width,int &height,float &minX,float &maxX,float &minY,float &maxY){
    return generate(Preview::read(("maps/"+map+".bsp").c_str()),rgba,width,height,minX,maxX,minY,maxY);
}
// Largest texture side for the isometric map; set from GL_MAX_TEXTURE_SIZE on the
// render thread, because the builder thread has no GL context.
static int textureCap=2048;
// Build an isometric, fully textured world overview from every visible BSP face.
// Camera basis is fixed so the complete map remains legible while the radar
// still shows a stable overview instead of rotating with the local player.
static bool generate3D(const std::vector<byte> &data,std::vector<byte> &rgba,int &width,int &height,float &minX,float &maxX,float &minY,float &maxY){
    if(data.size()<sizeof(Header))return false;
    Header header;memcpy(&header,data.data(),sizeof(header));if(header.version!=30)return false;
    size_t nv,ne,ns,nf,np,nt,nm;
    auto vertices=lump<Vertex>(data,header,3,nv);auto edges=lump<Edge>(data,header,12,ne);
    auto surf=lump<int32_t>(data,header,13,ns);auto faces=lump<Face>(data,header,7,nf);
    auto planes=lump<Plane>(data,header,1,np);auto texinfo=lump<TexInfo>(data,header,6,nt);auto models=lump<World>(data,header,14,nm);
    if(!vertices||!edges||!surf||!faces||!planes||!texinfo||!models||nv>65536||nf>65536||ne>1000000||ns>2000000)return false;
    const auto &world=models[0];if(world.first<0||world.count<0||static_cast<size_t>(world.first)+world.count>nf)return false;
    auto materials=RadarTextures::load(data,header.lumps[2].offset,header.lumps[2].length,header.lumps[0].offset,header.lumps[0].length);
    constexpr float invSqrt2=.70710678118f,invSqrt6=.40824829046f,invSqrt3=.57735026919f;
    struct Point {float x,y,z,s,t,d,px,py;};
    auto project=[&](float x,float y,float z){return Point{x,y,z,(x-y)*invSqrt2,-(x+y)*invSqrt6+z*2*invSqrt6,(x+y+z)*invSqrt3,0,0};};
    float loS=1e30f,hiS=-1e30f,loT=1e30f,hiT=-1e30f;
    for(int i=0;i<8;++i){float x=(i&1)?world.maxs[0]:world.mins[0],y=(i&2)?world.maxs[1]:world.mins[1],z=(i&4)?world.maxs[2]:world.mins[2];auto p=project(x,y,z);loS=std::min(loS,p.s);hiS=std::max(hiS,p.s);loT=std::min(loT,p.t);hiT=std::max(hiT,p.t);}
    if(!std::isfinite(loS)||!std::isfinite(hiS)||!std::isfinite(loT)||!std::isfinite(hiT)||hiS-loS<1||hiT-loT<1||hiS-loS>120000||hiT-loT>120000)return false;
    float side=std::max(hiS-loS,hiT-loT);if(!std::isfinite(side)||side<1)return false;
    float centerS=(loS+hiS)*.5f,centerT=(loT+hiT)*.5f;minX=centerS-side*.5f;maxX=centerS+side*.5f;minY=centerT-side*.5f;maxY=centerT+side*.5f;
    GLint cap=textureCap;if(wglGetCurrentContext())glGetIntegerv(GL_MAX_TEXTURE_SIZE,&cap);width=height=1;while(width*2<=std::min(2048,std::max(64,cap)))width=height=width*2;
    const size_t pixels=static_cast<size_t>(width)*height;std::vector<float> depth(pixels,-1e30f);std::vector<byte> color(pixels*4,0);size_t work=0,painted=0;
    const float scale=(width-2.f)/side,offsetX=width*.5f-centerS*scale,offsetY=height*.5f-centerT*scale;
    auto edge=[](const Point &a,const Point &b,float x,float y){return (b.px-a.px)*(y-a.py)-(b.py-a.py)*(x-a.px);};
    auto entities=parseEntities(data,header);int owner=0;auto parts=drawModels(entities,models,nm,nf,owner);auto propList=props(entities,owner);
    for(const auto &part:parts)for(int fi=part.first;fi<part.first+part.count;++fi){const auto &face=faces[fi];if(cancel.load(std::memory_order_relaxed))return false;
        if(face.plane>=np||face.texture<0||static_cast<size_t>(face.texture)>=nt||face.first<0||face.count<3||face.count>1024||static_cast<size_t>(face.first)+face.count>ns)continue;
        const auto &mapping=texinfo[face.texture];if(hiddenFace(materials,mapping))continue; // sky and tool textures
        const RadarTextures::Image *material=mapping.texture>=0&&static_cast<size_t>(mapping.texture)<materials.size()&&!materials[mapping.texture].rgb.empty()?&materials[mapping.texture]:nullptr;
        std::vector<Point> polygon;polygon.reserve(face.count);bool valid=true;
        for(int i=0;i<face.count;++i){int64_t signedEdge=surf[face.first+i];uint64_t ei=signedEdge<0?-signedEdge:signedEdge;if(ei>=ne){valid=false;break;}
            int vi=edges[ei].v[signedEdge<0?1:0];if(static_cast<size_t>(vi)>=nv){valid=false;break;}const auto &v=vertices[vi];
            if(!std::isfinite(v.p[0])||!std::isfinite(v.p[1])||!std::isfinite(v.p[2])||std::abs(v.p[0])>65536||std::abs(v.p[1])>65536||std::abs(v.p[2])>65536){valid=false;break;}
            auto p=project(v.p[0]+part.offset[0],v.p[1]+part.offset[1],v.p[2]+part.offset[2]);p.px=p.s*scale+offsetX;p.py=height-(p.t*scale+offsetY);polygon.push_back(p);
        }if(!valid)continue;
        for(size_t tri=1;tri+1<polygon.size();++tri){const Point &a=polygon[0],&b=polygon[tri],&c=polygon[tri+1];float area=edge(a,b,c.px,c.py);if(std::abs(area)<.001f)continue;
            int left=std::max(0,static_cast<int>(std::floor(std::min({a.px,b.px,c.px})))),right=std::min(width-1,static_cast<int>(std::ceil(std::max({a.px,b.px,c.px}))));
            int top=std::max(0,static_cast<int>(std::floor(std::min({a.py,b.py,c.py})))),bottom=std::min(height-1,static_cast<int>(std::ceil(std::max({a.py,b.py,c.py}))));if(left>right||top>bottom)continue;
            work+=static_cast<size_t>(right-left+1)*(bottom-top+1);if(work>1200000000)return false;
            for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){float wa=edge(b,c,x+.5f,y+.5f)/area,wb=edge(c,a,x+.5f,y+.5f)/area,wc=1-wa-wb;if(wa<-.001f||wb<-.001f||wc<-.001f)continue;
                float z=wa*a.d+wb*b.d+wc*c.d;size_t at=static_cast<size_t>(y)*width+x;if(z<=depth[at])continue;depth[at]=z;++painted;byte *dst=color.data()+at*4;
                if(material){float wx=wa*a.x+wb*b.x+wc*c.x-part.offset[0],wy=wa*a.y+wb*b.y+wc*c.y-part.offset[1],wz=wa*a.z+wb*b.z+wc*c.z-part.offset[2];
                    float tx=wx*mapping.vectors[0][0]+wy*mapping.vectors[0][1]+wz*mapping.vectors[0][2]+mapping.vectors[0][3];
                    float ty=wx*mapping.vectors[1][0]+wy*mapping.vectors[1][1]+wz*mapping.vectors[1][2]+mapping.vectors[1][3];
                    if(std::isfinite(tx)&&std::isfinite(ty)&&std::abs(tx)<1e9f&&std::abs(ty)<1e9f){int sx=static_cast<int>(std::floor(tx))%material->width,sy=static_cast<int>(std::floor(ty))%material->height;if(sx<0)sx+=material->width;if(sy<0)sy+=material->height;const byte *src=material->rgb.data()+(static_cast<size_t>(sy)*material->width+sx)*3;dst[0]=src[0];dst[1]=src[1];dst[2]=src[2];}
                    else {dst[0]=76;dst[1]=92;dst[2]=102;}
                }else {dst[0]=76;dst[1]=92;dst[2]=102;}dst[3]=255;
            }
        }
    }
    // Model props as their top footprint in the isometric view.
    for(const auto &prop:propList){
        Point q[4];for(int i=0;i<4;++i){q[i]=project(prop.corner[i][0],prop.corner[i][1],prop.top);q[i].px=q[i].s*scale+offsetX;q[i].py=height-(q[i].t*scale+offsetY);}
        for(int tri=0;tri<2;++tri){const Point &a=q[0],&b=q[tri+1],&c=q[tri+2];float area=edge(a,b,c.px,c.py);if(std::abs(area)<.001f)continue;
            int left=std::max(0,static_cast<int>(std::floor(std::min({a.px,b.px,c.px})))),right=std::min(width-1,static_cast<int>(std::ceil(std::max({a.px,b.px,c.px}))));
            int top=std::max(0,static_cast<int>(std::floor(std::min({a.py,b.py,c.py})))),bottom=std::min(height-1,static_cast<int>(std::ceil(std::max({a.py,b.py,c.py}))));if(left>right||top>bottom)continue;
            work+=static_cast<size_t>(right-left+1)*(bottom-top+1);if(work>1200000000)return false;
            for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){float wa=edge(b,c,x+.5f,y+.5f)/area,wb=edge(c,a,x+.5f,y+.5f)/area,wc=1-wa-wb;if(wa<-.001f||wb<-.001f||wc<-.001f)continue;
                float d=wa*a.d+wb*b.d+wc*c.d;size_t at=static_cast<size_t>(y)*width+x;if(d<=depth[at])continue;depth[at]=d;++painted;byte *dst=color.data()+at*4;dst[0]=122;dst[1]=112;dst[2]=94;dst[3]=255;}
        }
    }
    if(painted<100)return false;rgba=std::move(color);char message[128];int decoded=0;for(const auto &m:materials)if(!m.rgb.empty())++decoded;
    snprintf(message,sizeof(message),"Radar: rendered isometric 3D BSP map (%d decoded BSP/WAD textures).",decoded);logLine(message);return true;
}
static bool generate3D(const std::string &map,std::vector<byte> &rgba,int &width,int &height,float &minX,float &maxX,float &minY,float &maxY){
    return generate3D(Preview::read(("maps/"+map+".bsp").c_str()),rgba,width,height,minX,maxX,minY,maxY);
}
}
