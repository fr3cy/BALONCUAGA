#pragma once
// GoldSrc names its textures with a running counter and binds them without asking
// glGenTextures. A name handed out by glGenTextures is the next free one, so the game's
// next texture (a model, sprite or spray loaded later) took the same name: the radar or
// menu then showed the game's texture or nothing, and deleting it removed the game's.
// The plugin's textures take names far above anything the game's counter reaches.
namespace PluginGL {
static constexpr GLuint firstName=0x00100000;
static GLuint nextName=firstName;
static void APIENTRY genTextures(GLsizei count,GLuint *names){
    for(GLsizei i=0;i<count;++i){GLuint name;
        do{name=nextName++;if(nextName<firstName)nextName=firstName;}while(glIsTexture(name));   // skip a name someone already uses
        names[i]=name;}
}
}
#define glGenTextures PluginGL::genTextures
