#pragma once
// ---- 3D radar ------------------------------------------------------------------------------
// The radar as a small 3D diorama: the map's own faces (walls included) with its own
// textures, seen from above at an angle, cut to a disc around the player (or the whole map
// when the map is centred). Faces are prepared once per map on a worker thread and drawn
// from client arrays; a texture mask cuts the disc in world space (walls on the edge are
// cut straight down), and inside a building a clip plane removes everything over the head.
namespace Radar3D {
struct Vertex {float x,y,z,u,v;byte r,g,b,a;};
// Triangles of one material in one cell; `first`/`count` index Mesh::vertices once built.
// The bounds decide visibility, so a large face (a whole street) is drawn wherever it reaches.
struct Batch {int material=-1;size_t first=0,count=0;float minX=1e30f,minY=1e30f,maxX=-1e30f,maxY=-1e30f,minZ=1e30f;std::vector<Vertex> vertices;};
struct Cell {std::vector<Batch> batches;};
struct Material {std::vector<std::vector<byte>> levels;std::vector<int> widths,heights;};
// released: the CPU copies of the materials and vertices are gone once they are in video memory.
struct Mesh {bool ok=false,released=false;std::string map;float minX=0,minY=0,maxX=0,maxY=0,floorZ=0,topZ=0;int cellsX=0,cellsY=0;
    std::vector<Cell> cells;std::vector<Vertex> vertices;std::vector<Material> materials;size_t triangles=0;double ms=0;};
static constexpr float cellSize=512;
static constexpr float pitchDegrees=50,fovDegrees=30;
static constexpr int discSize=256;static constexpr float discRadius=discSize*.5f-1;   // disc mask, texels
static HANDLE thread;static std::atomic<bool> finished{false};
static std::shared_ptr<BspRadar::Scene> jobScene;static std::string jobMap;static Mesh result;
static std::shared_ptr<Mesh> mesh;
// GL objects of the current context; the 3D view is shown once every texture is in.
static HGLRC context;static std::vector<GLuint> textures;static size_t uploadMaterial;static int uploadLevel;static GLuint whiteTexture,discTexture;
static const Mesh *uploadedFor;static bool ready;
// The map's vertices in one buffer object (GL 1.5 / ARB_vertex_buffer_object) when available.
using GenBuffers=void(APIENTRY *)(GLsizei,GLuint *);using BindBuffer=void(APIENTRY *)(GLenum,GLuint);using BufferData=void(APIENTRY *)(GLenum,ptrdiff_t,const void *,GLenum);
static GenBuffers genBuffers,deleteBuffers;static BindBuffer bindBuffer;static BufferData bufferData;static GLuint vertexBuffer;
static bool borderClamp;   // GL 1.3: the building mask is clear outside its texture
// Where the cut runs through walls and solid blocks of the building the player is in (its
// window is kept alive while cached).
static GLuint capTexture;static const RadarPatch::Piece *capPiece;static std::shared_ptr<RadarPatch::Result> capOwner;
// The disc's own texture (see ensureFramebuffer) and what it shows.
using FbGenFn=void(APIENTRY *)(GLsizei,GLuint *);using FbBindFn=void(APIENTRY *)(GLenum,GLuint);
using FbTexFn=void(APIENTRY *)(GLenum,GLenum,GLenum,GLuint,GLint);using FbStoreFn=void(APIENTRY *)(GLenum,GLenum,GLsizei,GLsizei);
using FbAttachFn=void(APIENTRY *)(GLenum,GLenum,GLenum,GLuint);using FbStatusFn=GLenum(APIENTRY *)(GLenum);
struct FramebufferApi {FbGenFn gen,del,genRb,delRb;FbBindFn bind,bindRb;FbTexFn texture;FbStoreFn storage;FbAttachFn attach;FbStatusFn status;};
static FramebufferApi fb;static HGLRC fbContext;static bool fbUsable;
static GLuint fbo,fboColor,fboDepth;static int fboSize;static unsigned redraws;
struct Shot {float target[3]={},R=0,yaw=0,cut=0,low=0;const void *piece=nullptr,*window=nullptr,*map=nullptr;bool hideOutside=false,bridging=false;int size=0,style=0;bool ok=false;};
static Shot drawn;static double drawnAt;   // ms (QueryPerformanceCounter; GetTickCount64 steps by 16 ms)
static bool sameShot(const Shot &a,const Shot &b){
    return a.ok&&b.ok&&a.size==b.size&&a.style==b.style&&a.map==b.map&&a.window==b.window&&a.piece==b.piece&&a.hideOutside==b.hideOutside&&a.bridging==b.bridging&&std::fabs(a.cut-b.cut)<1&&std::fabs(a.low-b.low)<1&&
        std::fabs(a.target[0]-b.target[0])<.5f&&std::fabs(a.target[1]-b.target[1])<.5f&&std::fabs(a.target[2]-b.target[2])<.5f&&std::fabs(a.R-b.R)<.5f&&std::fabs(a.yaw-b.yaw)<.002f;}
static void freeFramebuffer(){
    if(fbContext==wglGetCurrentContext()&&fbUsable){if(fbo)fb.del(1,&fbo);if(fboDepth)fb.delRb(1,&fboDepth);if(fboColor)glDeleteTextures(1,&fboColor);}
    fbo=fboColor=fboDepth=0;fboSize=0;drawn=Shot{};
}
static void freeTextures(){
    freeFramebuffer();
    if(context==wglGetCurrentContext()){for(GLuint t:textures)if(t)glDeleteTextures(1,&t);if(whiteTexture)glDeleteTextures(1,&whiteTexture);if(discTexture)glDeleteTextures(1,&discTexture);
        if(capTexture)glDeleteTextures(1,&capTexture);if(vertexBuffer&&deleteBuffers)deleteBuffers(1,&vertexBuffer);}
    textures.clear();whiteTexture=discTexture=vertexBuffer=capTexture=0;capPiece=nullptr;capOwner.reset();uploadedFor=nullptr;uploadMaterial=0;uploadLevel=0;ready=false;
    if(mesh&&mesh->released)mesh.reset();   // nothing left to upload again from: it is built anew
}
static void stop(){if(thread){BspRadar::cancel=true;WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;}finished=false;jobScene.reset();result=Mesh{};mesh.reset();capOwner.reset();capPiece=nullptr;}

// Material texture: power of two (old GL), at most 256 texels a side, with mip levels.
static void prepareMaterial(const RadarTextures::Image &src,Material &out){
    if(src.rgb.empty()||src.width<1||src.height<1)return;
    int w=1,h=1;while(w<src.width&&w<256)w*=2;while(h<src.height&&h<256)h*=2;
    std::vector<byte> rgba(static_cast<size_t>(w)*h*4);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){size_t s=static_cast<size_t>(y*src.height/h)*src.width+x*src.width/w;byte *d=rgba.data()+(static_cast<size_t>(y)*w+x)*4;
        memcpy(d,src.rgb.data()+s*3,3);d[3]=src.alpha.empty()?255:src.alpha[s];if(!d[3])d[0]=d[1]=d[2]=0;}
    mipLevels(std::move(rgba),w,h,out.levels,out.widths,out.heights);
}
// Worker thread: every visible face as triangles with texture coordinates and a light
// shade, grouped by 512-unit cells and by material.
static void build(const BspRadar::Scene &s,Mesh &out){
    LARGE_INTEGER started,ended,frequency;QueryPerformanceCounter(&started);
    out.minX=s.minX;out.minY=s.minY;out.maxX=s.maxX;out.maxY=s.maxY;out.floorZ=s.floorZ;out.topZ=s.floorZ;
    out.cellsX=std::max(1,static_cast<int>(std::ceil((s.maxX-s.minX)/cellSize)));out.cellsY=std::max(1,static_cast<int>(std::ceil((s.maxY-s.minY)/cellSize)));
    out.cells.assign(static_cast<size_t>(out.cellsX)*out.cellsY,Cell{});
    const int materials=static_cast<int>(s.materials.size());std::vector<int> slot(out.cells.size()*(materials+1),-1);
    float light[3]={.35f,-.45f,.82f};{float l=std::sqrt(light[0]*light[0]+light[1]*light[1]+light[2]*light[2]);for(float &c:light)c/=l;}
    auto shade=[&](const float n[3]){float d=n[0]*light[0]+n[1]*light[1]+n[2]*light[2];return static_cast<byte>(std::clamp(255.f*(.36f+.64f*std::max(0.f,d)),0.f,255.f));};
    auto add=[&](const Vertex &a,const Vertex &b,const Vertex &c,int material){
        float cx=(a.x+b.x+c.x)/3,cy=(a.y+b.y+c.y)/3;
        int ix=std::clamp(static_cast<int>((cx-out.minX)/cellSize),0,out.cellsX-1),iy=std::clamp(static_cast<int>((cy-out.minY)/cellSize),0,out.cellsY-1);
        size_t cell=static_cast<size_t>(iy)*out.cellsX+ix;int &index=slot[cell*(materials+1)+(material+1)];auto &batches=out.cells[cell].batches;
        if(index<0){index=static_cast<int>(batches.size());batches.push_back(Batch{material,{}});}
        auto &batch=batches[index];auto &v=batch.vertices;v.push_back(a);v.push_back(b);v.push_back(c);++out.triangles;
        batch.minX=std::min({batch.minX,a.x,b.x,c.x});batch.maxX=std::max({batch.maxX,a.x,b.x,c.x});batch.minY=std::min({batch.minY,a.y,b.y,c.y});batch.maxY=std::max({batch.maxY,a.y,b.y,c.y});
        batch.minZ=std::min({batch.minZ,a.z,b.z,c.z});out.topZ=std::max({out.topZ,a.z,b.z,c.z});};
    std::vector<Vertex> polygon;std::vector<float> local;
    for(const auto &part:s.parts)for(int fi=part.first;fi<part.first+part.count;++fi){
        if(BspRadar::cancel.load(std::memory_order_relaxed))return;const auto &face=s.faces[fi];
        if(face.plane>=s.np||face.texture<0||static_cast<size_t>(face.texture)>=s.nt||face.first<0||face.count<3||face.count>1024||static_cast<size_t>(face.first)+face.count>s.ns)continue;
        const auto &mapping=s.textures[face.texture];if(BspRadar::hiddenFace(s.materials,mapping))continue;
        float sign=face.side?-1.f:1.f,n[3]={s.planes[face.plane].normal[0]*sign,s.planes[face.plane].normal[1]*sign,s.planes[face.plane].normal[2]*sign};
        if(!std::isfinite(n[0])||!std::isfinite(n[1])||!std::isfinite(n[2]))continue;
        int material=mapping.texture>=0&&mapping.texture<materials&&!s.materials[mapping.texture].rgb.empty()?mapping.texture:-1;
        float tw=material>=0?static_cast<float>(s.materials[material].width):64.f,th=material>=0?static_cast<float>(s.materials[material].height):64.f;
        byte light8=shade(n);polygon.clear();bool valid=true;
        for(int i=0;i<face.count;++i){int64_t edge=s.surf[face.first+i];uint64_t index=edge<0?-edge:edge;
            if(index>=s.ne){valid=false;break;}int vertex=s.edges[index].v[edge<0?1:0];if(static_cast<size_t>(vertex)>=s.nv){valid=false;break;}
            const auto &p=s.vertices[vertex].p;if(!std::isfinite(p[0])||!std::isfinite(p[1])||!std::isfinite(p[2])||std::abs(p[0])>65536||std::abs(p[1])>65536||std::abs(p[2])>65536){valid=false;break;}
            float u=(p[0]*mapping.vectors[0][0]+p[1]*mapping.vectors[0][1]+p[2]*mapping.vectors[0][2]+mapping.vectors[0][3])/tw;
            float v=(p[0]*mapping.vectors[1][0]+p[1]*mapping.vectors[1][1]+p[2]*mapping.vectors[1][2]+mapping.vectors[1][3])/th;
            polygon.push_back({p[0]+part.offset[0],p[1]+part.offset[1],p[2]+part.offset[2],u,v,light8,light8,light8,255});}
        if(!valid)continue;
        for(size_t i=1;i+1<polygon.size();++i){Vertex a=polygon[0],b=polygon[i],c=polygon[i+1];
            // Counter-clockwise seen from the face's front, so back faces can be culled.
            float e1[3]={b.x-a.x,b.y-a.y,b.z-a.z},e2[3]={c.x-a.x,c.y-a.y,c.z-a.z};
            float cr[3]={e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0]};
            if(cr[0]*n[0]+cr[1]*n[1]+cr[2]*n[2]<0)std::swap(b,c);add(a,b,c,material);}
    }
    // Model props (barrels, trees, cars): boxes of their footprint in a warm neutral tone.
    for(const auto &prop:s.props){
        if(!(prop.top>prop.bottom))continue;float zs[2]={prop.bottom,prop.top};
        auto corner=[&](int i,int level,const float n[3]){byte l=shade(n);return Vertex{prop.corner[i&3][0],prop.corner[i&3][1],zs[level],0,0,static_cast<byte>(122*l/255),static_cast<byte>(112*l/255),static_cast<byte>(94*l/255),255};};
        const float up[3]={0,0,1};Vertex top[4];for(int i=0;i<4;++i)top[i]=corner(i,1,up);
        float e1[3]={top[1].x-top[0].x,top[1].y-top[0].y,0},e2[3]={top[2].x-top[0].x,top[2].y-top[0].y,0};bool ccw=e1[0]*e2[1]-e1[1]*e2[0]>0;
        if(ccw){add(top[0],top[1],top[2],-1);add(top[0],top[2],top[3],-1);}else{add(top[0],top[2],top[1],-1);add(top[0],top[3],top[2],-1);}
        for(int i=0;i<4;++i){int j=(i+1)%4;float dx=prop.corner[j][0]-prop.corner[i][0],dy=prop.corner[j][1]-prop.corner[i][1],l=std::sqrt(dx*dx+dy*dy);if(l<.01f)continue;
            float n[3]={ccw?dy/l:-dy/l,ccw?-dx/l:dx/l,0};Vertex a=corner(i,0,n),b=corner(j,0,n),c=corner(j,1,n),d=corner(i,1,n);
            add(a,b,c,-1);add(a,c,d,-1);}
    }
    // One vertex array for the whole map (uploaded once to a buffer object).
    out.vertices.reserve(out.triangles*3);
    for(auto &cell:out.cells)for(auto &b:cell.batches){b.first=out.vertices.size();b.count=b.vertices.size();out.vertices.insert(out.vertices.end(),b.vertices.begin(),b.vertices.end());std::vector<Vertex>().swap(b.vertices);}
    out.materials.resize(materials);for(int m=0;m<materials;++m){if(BspRadar::cancel)return;prepareMaterial(s.materials[m],out.materials[m]);}
    out.ok=true;QueryPerformanceCounter(&ended);QueryPerformanceFrequency(&frequency);out.ms=1000.0*(ended.QuadPart-started.QuadPart)/frequency.QuadPart;
}
static DWORD WINAPI run(void *){Mesh out;build(*jobScene,out);out.map=jobMap;result=std::move(out);finished.store(true,std::memory_order_release);return 0;}

// Column-major 4x4 matrices as OpenGL takes them.
struct Matrix {float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};};
static Matrix multiply(const Matrix &a,const Matrix &b){Matrix r;for(int c=0;c<4;++c)for(int row=0;row<4;++row){float s=0;for(int k=0;k<4;++k)s+=a.m[k*4+row]*b.m[c*4+k];r.m[c*4+row]=s;}return r;}
static Matrix lookAt(const float eye[3],const float at[3]){
    float f[3]={at[0]-eye[0],at[1]-eye[1],at[2]-eye[2]};float l=std::sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);for(float &c:f)c/=l;
    float s[3]={f[1],-f[0],0};l=std::sqrt(s[0]*s[0]+s[1]*s[1]);s[0]/=l;s[1]/=l;   // f x up(0,0,1)
    float u[3]={s[1]*f[2]-s[2]*f[1],s[2]*f[0]-s[0]*f[2],s[0]*f[1]-s[1]*f[0]};
    Matrix r;r.m[0]=s[0];r.m[4]=s[1];r.m[8]=s[2];r.m[1]=u[0];r.m[5]=u[1];r.m[9]=u[2];r.m[2]=-f[0];r.m[6]=-f[1];r.m[10]=-f[2];
    r.m[12]=-(s[0]*eye[0]+s[1]*eye[1]+s[2]*eye[2]);r.m[13]=-(u[0]*eye[0]+u[1]*eye[1]+u[2]*eye[2]);r.m[14]=f[0]*eye[0]+f[1]*eye[1]+f[2]*eye[2];return r;
}
static Matrix frustum(float l,float r,float b,float t,float n,float f){
    Matrix m;for(float &v:m.m)v=0;m.m[0]=2*n/(r-l);m.m[5]=2*n/(t-b);m.m[8]=(r+l)/(r-l);m.m[9]=(t+b)/(t-b);m.m[10]=-(f+n)/(f-n);m.m[11]=-1;m.m[14]=-2*f*n/(f-n);return m;
}
// The camera of one frame; projects world points to the 2D overlay (y down).
struct View {Matrix projection,view,both;int vx=0,vy=0,size=0,screenH=0;float target[3]={},radius=0,yaw=0;
    bool project(float x,float y,float z,float &sx,float &sy)const{
        const float *m=both.m;float cx=m[0]*x+m[4]*y+m[8]*z+m[12],cy=m[1]*x+m[5]*y+m[9]*z+m[13],cw=m[3]*x+m[7]*y+m[11]*z+m[15];
        if(cw<=1e-4f)return false;sx=vx+(cx/cw+1)*.5f*size;sy=static_cast<float>(screenH)-(vy+(cy/cw+1)*.5f*size);return true;}};

// Render thread: keeps the mesh of the current map and its textures; true once all are in.
static bool prepare(){
    auto img=RadarJob::image;
    if(context!=wglGetCurrentContext()){textures.clear();whiteTexture=discTexture=vertexBuffer=capTexture=0;capPiece=nullptr;capOwner.reset();uploadedFor=nullptr;ready=false;context=wglGetCurrentContext();if(mesh&&mesh->released)mesh.reset();
        auto proc=[](const char *name)->PROC{PROC p=wglGetProcAddress(name);return reinterpret_cast<uintptr_t>(p)>4&&reinterpret_cast<uintptr_t>(p)!=static_cast<uintptr_t>(-1)?p:nullptr;};
        genBuffers=reinterpret_cast<GenBuffers>(proc("glGenBuffersARB"));deleteBuffers=reinterpret_cast<GenBuffers>(proc("glDeleteBuffersARB"));
        bindBuffer=reinterpret_cast<BindBuffer>(proc("glBindBufferARB"));bufferData=reinterpret_cast<BufferData>(proc("glBufferDataARB"));
        if(!genBuffers||!deleteBuffers||!bindBuffer||!bufferData)genBuffers=nullptr;
        const char *version=reinterpret_cast<const char *>(glGetString(GL_VERSION)),*extensions=reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));int major=0,minor=0;
        borderClamp=(version&&sscanf(version,"%d.%d",&major,&minor)==2&&(major>1||minor>=3))||(extensions&&strstr(extensions,"GL_ARB_texture_border_clamp"));}
    if(thread&&finished.load(std::memory_order_acquire)){WaitForSingleObject(thread,INFINITE);CloseHandle(thread);thread=nullptr;finished=false;
        auto done=std::make_shared<Mesh>(std::move(result));result=Mesh{};jobScene.reset();if(done->ok){mesh=done;ready=false;}}
    if(!img||!img->scene||RadarJob::imageMap!=overview.map)return false;
    if(!mesh||mesh->map!=overview.map){if(!thread&&!BspRadar::cancel){jobScene=img->scene;jobMap=overview.map;finished=false;thread=CreateThread(nullptr,0,run,nullptr,0,nullptr);if(thread)SetThreadPriority(thread,THREAD_PRIORITY_BELOW_NORMAL);}return false;}
    if(ready&&uploadedFor==mesh.get())return true;
    if(uploadedFor!=mesh.get()){for(GLuint t:textures)if(t)glDeleteTextures(1,&t);textures.assign(mesh->materials.size(),0);uploadMaterial=0;uploadLevel=0;uploadedFor=mesh.get();ready=false;
        if(vertexBuffer&&deleteBuffers)deleteBuffers(1,&vertexBuffer);vertexBuffer=0;
        if(genBuffers&&!mesh->vertices.empty()){genBuffers(1,&vertexBuffer);bindBuffer(0x8892,vertexBuffer);
            bufferData(0x8892,static_cast<ptrdiff_t>(mesh->vertices.size()*sizeof(Vertex)),mesh->vertices.data(),0x88E4);bindBuffer(0x8892,0);}}   // ARRAY_BUFFER, STATIC_DRAW
    auto params=[](GLint wrap,GLint minFilter){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minFilter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);};
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    if(!whiteTexture){glGenTextures(1,&whiteTexture);glBindTexture(GL_TEXTURE_2D,whiteTexture);params(GL_REPEAT,GL_LINEAR);const byte white[4]={255,255,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);}
    if(!discTexture){std::vector<byte> disc(size_t(discSize)*discSize*4,255);const float c=discSize*.5f;   // edge texels stay clear: clamping repeats them
        for(int y=0;y<discSize;++y)for(int x=0;x<discSize;++x){float dx=x+.5f-c,dy=y+.5f-c;disc[(static_cast<size_t>(y)*discSize+x)*4+3]=dx*dx+dy*dy<=discRadius*discRadius?255:0;}
        glGenTextures(1,&discTexture);glBindTexture(GL_TEXTURE_2D,discTexture);params(0x812F,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,discSize,discSize,0,GL_RGBA,GL_UNSIGNED_BYTE,disc.data());}
    // Materials, about 1 MB per frame (the 2D radar is shown meanwhile).
    size_t budget=size_t(1)<<20;
    while(uploadMaterial<mesh->materials.size()&&budget){const auto &m=mesh->materials[uploadMaterial];
        if(m.levels.empty()){++uploadMaterial;continue;}
        if(!textures[uploadMaterial]){glGenTextures(1,&textures[uploadMaterial]);glBindTexture(GL_TEXTURE_2D,textures[uploadMaterial]);params(GL_REPEAT,GL_LINEAR_MIPMAP_LINEAR);uploadLevel=0;}
        glBindTexture(GL_TEXTURE_2D,textures[uploadMaterial]);
        while(uploadLevel<static_cast<int>(m.levels.size())&&budget){size_t bytes=static_cast<size_t>(m.widths[uploadLevel])*m.heights[uploadLevel]*4;
            glTexImage2D(GL_TEXTURE_2D,uploadLevel,GL_RGBA,m.widths[uploadLevel],m.heights[uploadLevel],0,GL_RGBA,GL_UNSIGNED_BYTE,m.levels[uploadLevel].data());
            budget=bytes>=budget?0:budget-bytes;++uploadLevel;}
        if(uploadLevel>=static_cast<int>(m.levels.size())){++uploadMaterial;uploadLevel=0;}
    }
    ready=uploadMaterial>=mesh->materials.size();
    // Everything is in video memory: the CPU copies (tens of MB on big maps, in a 32-bit game) go.
    if(ready&&!mesh->released){for(auto &material:mesh->materials)std::vector<std::vector<byte>>().swap(material.levels);if(vertexBuffer)std::vector<Vertex>().swap(mesh->vertices);mesh->released=true;}
    return ready;
}
// Texture unit `unit` multiplies the alpha so far by its texture's alpha (or 1 - alpha) and
// keeps the colour; its coordinates come from world x,y through `s` and `t`.
static void maskUnit(GLuint texture,const float s[4],const float t[4],bool inverse){
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,0x8570);                                  // GL_COMBINE
    glTexEnvi(GL_TEXTURE_ENV,0x8571,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,0x8580,0x8578);glTexEnvi(GL_TEXTURE_ENV,0x8590,GL_SRC_COLOR);   // rgb = previous
    glTexEnvi(GL_TEXTURE_ENV,0x8572,GL_MODULATE);glTexEnvi(GL_TEXTURE_ENV,0x8588,0x8578);glTexEnvi(GL_TEXTURE_ENV,0x8598,GL_SRC_ALPHA);  // alpha = previous *
    glTexEnvi(GL_TEXTURE_ENV,0x8589,GL_TEXTURE);glTexEnvi(GL_TEXTURE_ENV,0x8599,inverse?GL_ONE_MINUS_SRC_ALPHA:GL_SRC_ALPHA);        //         texture (or 1 - texture)
    glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);glTexGenfv(GL_S,GL_OBJECT_PLANE,s);glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);glTexGenfv(GL_T,GL_OBJECT_PLANE,t);
    glEnable(GL_TEXTURE_GEN_S);glEnable(GL_TEXTURE_GEN_T);
}
static void unitOff(){glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);glDisable(GL_TEXTURE_2D);}
using ActiveFn=void(APIENTRY *)(GLenum);
// ---- A texture of its own --------------------------------------------------------------
// The disc is drawn into a texture (GL_EXT_framebuffer_object) when the view or the building
// changed, at most about 65 times a second, and the texture is laid into the radar's circle
// every frame: nothing leaves the circle, and a still view costs one textured circle.
static bool ensureFramebuffer(int size){
    const HGLRC current=wglGetCurrentContext();
    if(fbContext!=current){fbContext=current;fbo=fboColor=fboDepth=0;fboSize=0;drawn=Shot{};
        auto proc=[](const char *name)->PROC{PROC p=wglGetProcAddress(name);return reinterpret_cast<uintptr_t>(p)>4&&reinterpret_cast<uintptr_t>(p)!=static_cast<uintptr_t>(-1)?p:nullptr;};
        fb.gen=reinterpret_cast<FbGenFn>(proc("glGenFramebuffersEXT"));fb.del=reinterpret_cast<FbGenFn>(proc("glDeleteFramebuffersEXT"));
        fb.genRb=reinterpret_cast<FbGenFn>(proc("glGenRenderbuffersEXT"));fb.delRb=reinterpret_cast<FbGenFn>(proc("glDeleteRenderbuffersEXT"));
        fb.bind=reinterpret_cast<FbBindFn>(proc("glBindFramebufferEXT"));fb.bindRb=reinterpret_cast<FbBindFn>(proc("glBindRenderbufferEXT"));
        fb.texture=reinterpret_cast<FbTexFn>(proc("glFramebufferTexture2DEXT"));fb.storage=reinterpret_cast<FbStoreFn>(proc("glRenderbufferStorageEXT"));
        fb.attach=reinterpret_cast<FbAttachFn>(proc("glFramebufferRenderbufferEXT"));fb.status=reinterpret_cast<FbStatusFn>(proc("glCheckFramebufferStatusEXT"));
        fbUsable=fb.gen&&fb.del&&fb.genRb&&fb.delRb&&fb.bind&&fb.bindRb&&fb.texture&&fb.storage&&fb.attach&&fb.status;}
    if(!fbUsable)return false;
    int s=64;while(s<size&&s<2048)s*=2;if(s<size)return false;
    if(fbo&&fboSize==s)return true;
    freeFramebuffer();
    GLint previous=0;glGetIntegerv(0x8CA6,&previous);                                          // FRAMEBUFFER_BINDING_EXT
    glGenTextures(1,&fboColor);glBindTexture(GL_TEXTURE_2D,fboColor);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
    glTexImage2D(GL_TEXTURE_2D,0,0x8058,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);                 // RGBA8
    fb.genRb(1,&fboDepth);fb.bindRb(0x8D41,fboDepth);fb.storage(0x8D41,0x81A6,s,s);fb.bindRb(0x8D41,0);   // DEPTH_COMPONENT24
    fb.gen(1,&fbo);fb.bind(0x8D40,fbo);fb.texture(0x8D40,0x8CE0,GL_TEXTURE_2D,fboColor,0);fb.attach(0x8D40,0x8D00,0x8D41,fboDepth);
    const bool complete=fb.status(0x8D40)==0x8CD5;fb.bind(0x8D40,static_cast<GLuint>(previous));
    if(!complete){freeFramebuffer();fbUsable=false;logLine("3D radar: no usable framebuffer object; the disc is drawn directly, clipped to the circle.");return false;}
    fboSize=s;return true;
}
// Draws the disc. False while the mesh or its textures are not ready, or without
// multitexturing; the 2D radar is drawn then.
static bool draw(const SCREENINFO &screen,float cx,float cy,float radiusPx,const float *origin,float yawRadians,bool centered,float feetZ,View &v){
    auto active=reinterpret_cast<ActiveFn>(GL2D::procActive);if(!active||GL2D::procUnits<2)return false;
    static HGLRC clientContext;static ActiveFn clientActive;
    if(clientContext!=wglGetCurrentContext()){clientContext=wglGetCurrentContext();PROC p=wglGetProcAddress("glClientActiveTextureARB");
        clientActive=reinterpret_cast<uintptr_t>(p)>4&&reinterpret_cast<uintptr_t>(p)!=static_cast<uintptr_t>(-1)?reinterpret_cast<ActiveFn>(p):nullptr;}
    if(!prepare())return false;
    const Mesh &m=*mesh;auto img=RadarJob::image;
    // Camera: behind and above the player looking where they look (or north over the map's centre).
    float target[3],R,yaw;
    if(centered){target[0]=(m.minX+m.maxX)*.5f;target[1]=(m.minY+m.maxY)*.5f;target[2]=m.floorZ+(m.topZ-m.floorZ)*.25f;
        R=std::max(m.maxX-m.minX,m.maxY-m.minY)*.5f/(std::clamp(radarCenteredZoom,100,400)/100.f);yaw=1.5707963f;}
    else{target[0]=origin[0];target[1]=origin[1];target[2]=feetZ;R=std::max(100.f,static_cast<float>(range));yaw=yawRadians;}
    const float pitch=pitchDegrees*.017453293f,half=std::tan(fovDegrees*.5f*.017453293f),distance=R/half*1.1f;
    const float eye[3]={target[0]-std::cos(yaw)*std::cos(pitch)*distance,target[1]-std::sin(yaw)*std::cos(pitch)*distance,target[2]+std::sin(pitch)*distance};
    const float nearZ=distance*.3f,farZ=distance*3,w=nearZ*half;
    v.view=lookAt(eye,target);v.projection=frustum(-w,w,-w,w,nearZ,farZ);v.both=multiply(v.projection,v.view);   // the whole disc and its side fit, walls at the back up to about R/2
    v.size=std::max(8,static_cast<int>(radiusPx*2));v.vx=static_cast<int>(cx-radiusPx);v.vy=screen.iHeight-static_cast<int>(cy+radiusPx);v.screenH=screen.iHeight;
    for(int i=0;i<3;++i)v.target[i]=target[i];v.radius=R;v.yaw=yaw;
    // In a closed building the building's own texture from the 2D radar masks it: a plane above
    // the head removes its roof, one under the feet its lower storeys (drawn dark), and the
    // outside is hidden in "only the building". Under an arch, a porch or anything open the
    // map is drawn as it is.
    const RadarPatch::Texture *piece=RadarPatch::shown&&RadarPatch::shown->done&&RadarPatch::inBuilding&&!centered?RadarPatch::shown:nullptr;
    const bool hideOutside=piece&&radarInsideOnly;
    // The height the building's mask and caps were made for; while an older window bridges the
    // wait for a new one, the player's own height, with no caps and no storey below cut off.
    const bool bridging=piece&&RadarPatch::bridging;
    const float windowCutoff=piece&&!bridging?piece->owner->cutoff:RadarPatch::heldCutoff;
    const float cut=windowCutoff+8,low=bridging?-1e9f:windowCutoff-RadarPatch::aboveFeet-RadarPatch::floorDepth;
    // The disc itself, into the viewport at (vx, vy): the radar's own texture, or (without one)
    // the screen with everything outside the radar's circle masked off.
    auto renderDisc=[&](int vx,int vy,bool own){
    // Batches reaching into the disc (by their bounds, so a large face is never lost), sorted
    // by material; the disc's floor lies below them, but not so deep that the side becomes a well.
    static std::vector<const Batch *> visible;visible.clear();float lowest=target[2]-48;
    for(const auto &cell:m.cells)for(const auto &b:cell.batches){if(!b.count)continue;
        float nx=std::clamp(target[0],b.minX,b.maxX),ny=std::clamp(target[1],b.minY,b.maxY);
        if((nx-target[0])*(nx-target[0])+(ny-target[1])*(ny-target[1])>R*R)continue;visible.push_back(&b);lowest=std::min(lowest,b.minZ-8);}
    std::sort(visible.begin(),visible.end(),[](const Batch *a,const Batch *b){return a->material<b->material;});
    const float bottom=std::max(lowest,target[2]-std::max(64.f,R*.2f)),thickness=std::max(16.f,R*.07f);
    glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadMatrixf(v.projection.m);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadMatrixf(v.view.m);
    glViewport(vx,vy,v.size,v.size);glDepthMask(GL_TRUE);glDepthRange(0,1);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    if(own){glDisable(GL_SCISSOR_TEST);glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
    else{glEnable(GL_SCISSOR_TEST);glScissor(vx,vy,v.size,v.size);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);
        // Outside the radar's circle the depth is set nearest: nothing of the disc is drawn there.
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glDisable(GL_TEXTURE_2D);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
        glBegin(GL_TRIANGLE_STRIP);for(int i=0;i<=96;++i){float a=i*6.2831853f/96,c=std::cos(a),s=std::sin(a),edge=std::max(std::fabs(c),std::fabs(s));
            glVertex3f(c,s,-1);glVertex3f(c/edge,s/edge,-1);}glEnd();
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopMatrix();}
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.5f);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CCW);
    glShadeModel(GL_SMOOTH);glDisable(GL_LIGHTING);glDisable(GL_FOG);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glDisable(GL_COLOR_LOGIC_OP);
    for(int u=1;u<std::min(GL2D::procUnits,4);++u){active(0x84C0+u);unitOff();}
    active(0x84C0);unitOff();
    // Side of the disc: from below its floor up to the ground along the edge (never far over
    // the player's feet, so walls on the edge do not raise it into a cup).
    constexpr int n=128;float rim[n+1];
    auto edge=[&](int i,float &x,float &y){float a=i*6.2831853f/n;x=target[0]+std::cos(a)*R;y=target[1]+std::sin(a)*R;};
    {glDisable(GL_TEXTURE_2D);glDisable(GL_CULL_FACE);
        // Its floor first and without depth: seen through gaps, it hides nothing of the map.
        glDepthMask(GL_FALSE);glColor4ub(16,18,22,255);glBegin(GL_TRIANGLE_FAN);glVertex3f(target[0],target[1],bottom);
        for(int i=0;i<=n;++i){float x,y;edge(i,x,y);glVertex3f(x,y,bottom);}glEnd();glDepthMask(GL_TRUE);
        glBegin(GL_QUAD_STRIP);
        for(int i=0;i<=n;++i){float x,y;edge(i,x,y);float top=bottom;
            if(img&&img->top.size()==size_t(2048)*2048){int gx=static_cast<int>((x-img->minX)/(img->maxX-img->minX)*2047),gy=static_cast<int>((img->maxY-y)/(img->maxY-img->minY)*2047);
                if(gx>=0&&gy>=0&&gx<2048&&gy<2048){int16_t h=img->top[static_cast<size_t>(gy)*2048+gx];if(h!=-32768)top=static_cast<float>(h);}}
            top=std::min(top,target[2]+24);if(piece)top=std::min(top,cut);top=std::max(top,bottom);rim[i]=top;
            glColor4ub(26,26,30,255);glVertex3f(x,y,bottom-thickness);glColor4ub(58,54,50,255);glVertex3f(x,y,top);}
        glEnd();
        // Border colour and width from the radar settings, along the bottom of the side.
        if(radarBorderOn&&radarBorderOpacity>0){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthRange(0,.9995);
            glLineWidth(static_cast<float>(std::clamp(radarBorderWidth,1,10)));glColor4ub(radarBorderRgb[0],radarBorderRgb[1],radarBorderRgb[2],static_cast<byte>(std::clamp(radarBorderOpacity,0,100)*255/100));
            glBegin(GL_LINE_STRIP);for(int i=0;i<=n;++i){float x,y;edge(i,x,y);glVertex3f(x,y,bottom-thickness);}glEnd();
            glDepthRange(0,1);glDisable(GL_BLEND);glLineWidth(1);}
        glEnable(GL_CULL_FACE);
    }
    // The map: unit 0 texture x shade, unit 1 the disc, unit 2 the building (when inside).
    active(0x84C0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    const float k=discRadius/(float(discSize)*R),discS[4]={k,0,0,.5f-target[0]*k},discT[4]={0,k,0,.5f-target[1]*k};   // radius R = the disc's edge texels
    active(0x84C1);maskUnit(discTexture,discS,discT,false);
    float pieceS[4]={0,0,0,0},pieceT[4]={0,0,0,0};const bool pieceMask=piece&&GL2D::procUnits>=3&&img&&borderClamp;
    if(pieceMask){const auto &pc=*piece->piece;float spanX=img->maxX-img->minX,spanY=img->maxY-img->minY,bx=static_cast<float>(piece->owner->ox+pc.x),by=static_cast<float>(piece->owner->oy+pc.y);
        pieceS[0]=2048.f/(spanX*pc.w);pieceS[3]=(-img->minX*2048.f/spanX-bx)/pc.w;pieceT[1]=-2048.f/(spanY*pc.h);pieceT[3]=(img->maxY*2048.f/spanY-by)/pc.h;}
    if(clientActive)clientActive(0x84C0);
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glDisableClientState(GL_NORMAL_ARRAY);
    // One set of pointers for every pass: into the buffer object, or the mesh's own array.
    const bool buffered=vertexBuffer&&bindBuffer;if(buffered)bindBuffer(0x8892,vertexBuffer);
    auto at=[&](size_t offset)->const void *{return buffered?reinterpret_cast<const void *>(offset):reinterpret_cast<const char *>(m.vertices.data())+offset;};
    glVertexPointer(3,GL_FLOAT,sizeof(Vertex),at(offsetof(Vertex,x)));glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),at(offsetof(Vertex,u)));glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),at(offsetof(Vertex,r)));
    auto scene=[&](bool plain){active(0x84C0);GLuint bound=0;if(plain)glBindTexture(GL_TEXTURE_2D,whiteTexture);
        for(const Batch *b:visible){
            GLuint t=b->material>=0&&static_cast<size_t>(b->material)<textures.size()&&textures[b->material]?textures[b->material]:whiteTexture;
            if(!plain&&t!=bound){glBindTexture(GL_TEXTURE_2D,t);bound=t;}
            glDrawArrays(GL_TRIANGLES,static_cast<GLint>(b->first),static_cast<GLsizei>(b->count));}};
    const double overHead[4]={0,0,-1,cut},thisFloor[4]={0,0,1,-low},lower[4]={0,0,-1,low};   // world space: the view matrix is loaded
    glClipPlane(GL_CLIP_PLANE0,overHead);glClipPlane(GL_CLIP_PLANE1,thisFloor);glClipPlane(GL_CLIP_PLANE2,lower);
    if(!piece)scene(false);
    else if(pieceMask){
        // Outside its texture the building's mask is clear (the 2D radar clamps it to the edge).
        active(0x84C2);maskUnit(piece->id,pieceS,pieceT,false);const float clear[4]={0,0,0,0};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,clear);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812D);   // CLAMP_TO_BORDER
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);   // seen at an angle a blurred mip level would spread the mask past the walls
        // The building's passes keep only texels well inside its mask (alpha > .97, the rest > .03 below):
        // ground just outside an outer wall shares the wall's edge texel and stays out.
        glAlphaFunc(GL_GREATER,.97f);
        glEnable(GL_CLIP_PLANE0);glEnable(GL_CLIP_PLANE1);scene(false);glDisable(GL_CLIP_PLANE1);           // the building on this floor, roof removed
        // Lower storeys: only their shapes, dark and without textures (shade x a dim constant).
        active(0x84C0);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,0x8570);
        glTexEnvi(GL_TEXTURE_ENV,0x8571,GL_MODULATE);glTexEnvi(GL_TEXTURE_ENV,0x8580,0x8577);glTexEnvi(GL_TEXTURE_ENV,0x8590,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,0x8581,0x8576);glTexEnvi(GL_TEXTURE_ENV,0x8591,GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV,0x8572,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,0x8588,0x8577);glTexEnvi(GL_TEXTURE_ENV,0x8598,GL_SRC_ALPHA);
        const float dim[4]={.2f,.22f,.25f,1};glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,dim);
        glEnable(GL_CLIP_PLANE2);scene(true);glDisable(GL_CLIP_PLANE2);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        // Walls and solid blocks the cut runs through get a flat dark top (a cut section), instead
        // of showing their hollow inside; no 2D colours, which turned light roofs into white patches.
        const auto &pc=*piece->piece;
        if(!bridging&&pc.solid.size()==static_cast<size_t>(pc.w)*pc.h&&(capPiece!=&pc||capOwner!=piece->owner||!capTexture)){
            if(!capTexture)glGenTextures(1,&capTexture);active(0x84C1);glBindTexture(GL_TEXTURE_2D,capTexture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812D);
            glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
            glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,pc.w,pc.h,0,GL_ALPHA,GL_UNSIGNED_BYTE,pc.solid.data());capPiece=&pc;capOwner=piece->owner;}
        if(!bridging&&capTexture&&capPiece==&pc&&capOwner==piece->owner){
            glDisable(GL_CLIP_PLANE0);glAlphaFunc(GL_GREATER,.5f);   // caps: the wall's own outline
            active(0x84C0);glDisable(GL_TEXTURE_2D);
            active(0x84C1);maskUnit(capTexture,pieceS,pieceT,false);active(0x84C2);maskUnit(discTexture,discS,discT,false);
            const float spanX=img->maxX-img->minX,spanY=img->maxY-img->minY,bx=static_cast<float>(piece->owner->ox+pc.x),by=static_cast<float>(piece->owner->oy+pc.y);
            const float x0=img->minX+bx*spanX/2048,x1=img->minX+(bx+pc.w)*spanX/2048,y1=img->maxY-by*spanY/2048,y0=img->maxY-(by+pc.h)*spanY/2048;
            glDisableClientState(GL_COLOR_ARRAY);glColor4ub(52,50,48,255);glBegin(GL_QUADS);glVertex3f(x0,y0,cut);glVertex3f(x1,y0,cut);glVertex3f(x1,y1,cut);glVertex3f(x0,y1,cut);glEnd();glEnableClientState(GL_COLOR_ARRAY);
            active(0x84C0);glEnable(GL_TEXTURE_2D);
            active(0x84C1);maskUnit(discTexture,discS,discT,false);active(0x84C2);maskUnit(piece->id,pieceS,pieceT,false);}
        if(!hideOutside){glDisable(GL_CLIP_PLANE0);active(0x84C2);maskUnit(piece->id,pieceS,pieceT,true);glAlphaFunc(GL_GREATER,.03f);scene(false);}   // the rest as it is
        glAlphaFunc(GL_GREATER,.5f);
        active(0x84C2);glBindTexture(GL_TEXTURE_2D,piece->id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    }else{glEnable(GL_CLIP_PLANE0);scene(false);}                                                        // without the mask: cut everything over the head
    glDisable(GL_CLIP_PLANE0);glDisable(GL_CLIP_PLANE1);glDisable(GL_CLIP_PLANE2);if(buffered)bindBuffer(0x8892,0);
    glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_COLOR_ARRAY);
    for(int u=1;u<std::min(GL2D::procUnits,4);++u){active(0x84C0+u);unitOff();}active(0x84C0);unitOff();
    // A dark edge along the top of the side covers the steps where the mask cuts the map.
    glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);glDepthRange(0,.9995);glLineWidth(2);glColor4ub(30,28,26,255);
    glBegin(GL_LINE_STRIP);for(int i=0;i<=n;++i){float x,y;edge(i,x,y);glVertex3f(x,y,rim[i]);}glEnd();
    glLineWidth(1);glDepthRange(0,1);
    glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopMatrix();
    glPopClientAttrib();glPopAttrib();
    };
    if(!ensureFramebuffer(v.size)){renderDisc(v.vx,v.vy,false);return true;}
    // Drawn again only when something it shows changed; a moving view at most ~65 times a second.
    Shot shot;for(int i=0;i<3;++i)shot.target[i]=target[i];shot.R=R;shot.yaw=yaw;shot.cut=piece?cut:0;shot.low=piece?low:0;
    shot.piece=piece?piece->piece:nullptr;shot.window=piece?piece->owner.get():nullptr;shot.map=&m;shot.hideOutside=hideOutside;shot.bridging=bridging;shot.size=v.size;
    shot.style=(radarBorderOn?1:0)|std::clamp(radarBorderWidth,0,15)<<1|std::clamp(radarBorderOpacity,0,100)<<5|(radarBorderRgb[0]<<12^radarBorderRgb[1]<<18^radarBorderRgb[2]<<24);shot.ok=true;
    LARGE_INTEGER counter,frequency;QueryPerformanceCounter(&counter);QueryPerformanceFrequency(&frequency);const double now=counter.QuadPart*1000.0/frequency.QuadPart;
    const bool newScene=!drawn.ok||drawn.map!=shot.map||drawn.window!=shot.window||drawn.piece!=shot.piece||drawn.hideOutside!=shot.hideOutside||drawn.bridging!=shot.bridging||drawn.size!=shot.size||drawn.style!=shot.style;
    if(newScene||(!sameShot(drawn,shot)&&now-drawnAt>=15)){
        GLint previous=0;glGetIntegerv(0x8CA6,&previous);fb.bind(0x8D40,fbo);
        renderDisc(0,0,true);
        fb.bind(0x8D40,static_cast<GLuint>(previous));drawn=shot;drawnAt=now;++redraws;}
    // Into the radar's circle (screen y grows downwards; the texture's rows upwards).
    const float s=static_cast<float>(fboSize),left=static_cast<float>(v.vx),base=static_cast<float>(v.vy);
    auto texel=[&](float x,float y){glTexCoord2f((x-left)/s,(static_cast<float>(screen.iHeight)-y-base)/s);glVertex2f(x,y);};
    const GLboolean blend=glIsEnabled(GL_BLEND);
    active(0x84C0);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,fboColor);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4ub(255,255,255,255);
    const int segments=std::max(48,std::min(160,static_cast<int>(radiusPx/2)));
    glBegin(GL_TRIANGLE_FAN);texel(cx,cy);for(int i=0;i<=segments;++i){float a=i*6.2831853f/segments;texel(cx+std::cos(a)*radiusPx,cy+std::sin(a)*radiusPx);}glEnd();
    glDisable(GL_TEXTURE_2D);if(!blend)glDisable(GL_BLEND);
    return true;
}
}
