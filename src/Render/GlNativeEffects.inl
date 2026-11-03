// Shared implementation for the desktop GL and GLES backends. Included after
// GlesDevice::Impl so snapshots restore the actual pipeline state.
namespace oj::render {
NativeRenderDevice::Target GlesDevice::nativeTarget() const { return {p->target,p->face,p->viewport}; }
NativeRenderDevice::SavedState GlesDevice::saveNativeState() {
    const auto target=p->target,face=p->face,vs=p->vs,ps=p->ps,decl=p->decl,stream=p->stream,
               indices=p->indices,stride=p->stride,fvf=p->fvf;
    const auto states=p->states;const auto sampling=p->sampling;const auto stages=p->stages;const auto bound=p->bound;
    const auto viewport=p->viewport;const auto clip=p->clip;
    const bool clipped=p->clipped,nativePass=p->nativePass;
    // Native passes touch only c0..c255. The larger register banks used by
    // legacy game shaders stay untouched; avoid copying them per effect.
    std::array<float,1024> vertex{},pixel{};
    std::copy_n(p->vertexRegisters.floats.begin(),vertex.size(),vertex.begin());
    std::copy_n(p->pixelRegisters.floats.begin(),pixel.size(),pixel.begin());
    p->nativePass=true;
    return SavedState([this,target,face,vs,ps,decl,stream,indices,stride,fvf,states,sampling,stages,bound,
                       viewport,clip,clipped,nativePass,vertex,pixel] {
        p->target=target;p->face=face;p->vs=vs;p->ps=ps;p->decl=decl;p->stream=stream;
        p->indices=indices;p->stride=stride;p->fvf=fvf;p->states=states;p->sampling=sampling;
        p->stages=stages;p->bound=bound;p->viewport=viewport;p->clip=clip;p->clipped=clipped;p->nativePass=nativePass;
        std::copy(vertex.begin(),vertex.end(),p->vertexRegisters.floats.begin());
        std::copy(pixel.begin(),pixel.end(),p->pixelRegisters.floats.begin());
        p->framebuffer();
    });
}
TextureId GlesDevice::createNativeTarget(uint32_t w,uint32_t h,TargetFormat format) {
    if(std::this_thread::get_id()!=owner) return onOwner([&]{return createNativeTarget(w,h,format);});
    if(!w || !h) return 0;
    Impl::Texture t;t.w=w;t.h=h;t.scalar=format==TargetFormat::Scalar;t.hdr=format==TargetFormat::Hdr;
    glGenTextures(1,&t.gl);glBindTexture(GL_TEXTURE_2D,t.gl);
    glTexImage2D(GL_TEXTURE_2D,0,t.scalar?GL_R32F:t.hdr?GL_RGBA16F:GL_RGBA8,w,h,0,t.scalar?GL_RED:GL_RGBA,
                 t.scalar||t.hdr?GL_FLOAT:GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
    glGenRenderbuffers(1,&t.depth);glBindRenderbuffer(GL_RENDERBUFFER,t.depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
    glGenFramebuffers(1,&t.fbo);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.gl,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,t.depth);
    const bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    p->framebuffer();
    if(!ok) { glDeleteTextures(1,&t.gl);glDeleteFramebuffers(1,&t.fbo);glDeleteRenderbuffers(1,&t.depth);return 0; }
    const auto id=p->next++;p->textures.emplace(id,std::move(t));return id;
}
TextureId GlesDevice::createNativeScalarTexture(uint32_t w,uint32_t h,const float* values) {
    if(std::this_thread::get_id()!=owner) return onOwner([&]{return createNativeScalarTexture(w,h,values);});
    Impl::Texture t;t.w=w;t.h=h;t.scalar=true;
    glGenTextures(1,&t.gl);glBindTexture(GL_TEXTURE_2D,t.gl);
    // Weather requires filtered 16-bit precision to avoid cloud banding.
    glTexImage2D(GL_TEXTURE_2D,0,GL_R16F,w,h,0,GL_RED,GL_FLOAT,values);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
    if(glGetError()!=GL_NO_ERROR) { glDeleteTextures(1,&t.gl);return 0; }
    const auto id=p->next++;p->textures.emplace(id,std::move(t));return id;
}
TextureId GlesDevice::createNativeVolume(uint32_t w,uint32_t h,uint32_t depth,const float* rgba) {
    if(std::this_thread::get_id()!=owner) return onOwner([&]{return createNativeVolume(w,h,depth,rgba);});
    Impl::Texture t;t.w=w;t.h=h;t.volume=true;t.hdr=true;
    glGenTextures(1,&t.gl);glBindTexture(GL_TEXTURE_3D,t.gl);
    glTexImage3D(GL_TEXTURE_3D,0,GL_RGBA16F,w,h,depth,0,GL_RGBA,GL_FLOAT,rgba);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAX_LEVEL,0);
    if(glGetError()!=GL_NO_ERROR) { glDeleteTextures(1,&t.gl);return 0; }
    const auto id=p->next++;p->textures.emplace(id,std::move(t));return id;
}
bool GlesDevice::copyNativeTarget(TextureId from,uint32_t face,const Viewport& r,TextureId to) {
    if(!to || !p->textures.contains(to) || p->textures.at(to).volume || !r.width || !r.height) return false;
    const auto saved=p->target,savedFace=p->face;
    p->target=from;p->face=face;p->framebuffer();
    GLint source=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&source);
    p->target=to;p->face=0;p->framebuffer();
    glBindFramebuffer(GL_READ_FRAMEBUFFER,source);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,p->textures.at(to).fbo);
    glDisable(GL_SCISSOR_TEST);
    const auto height=from?p->textures.at(from).h:p->h;
    const int y0=from?int(r.y):int(height-r.y),y1=from?int(r.y+r.height):int(height-r.y-r.height);
    glBlitFramebuffer(r.x,y0,r.x+r.width,y1,0,0,p->textures.at(to).w,p->textures.at(to).h,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    const bool ok=glGetError()==GL_NO_ERROR;p->target=saved;p->face=savedFace;p->framebuffer();return ok;
}
bool GlesDevice::copyNativeDepth(TextureId from,TextureId to) {
    if(!to || !p->textures.contains(to)) return false;
    const auto w=targetWidth(from),h=targetHeight(from);
    if(targetWidth(to)!=w || targetHeight(to)!=h) return false;
    const auto saved=p->target,savedFace=p->face;
    p->target=from;p->face=0;p->framebuffer();GLint source=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&source);
    p->target=to;p->framebuffer();glBindFramebuffer(GL_READ_FRAMEBUFFER,source);
    glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0,from?0:h,w,from?h:0,0,0,w,h,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    const bool ok=glGetError()==GL_NO_ERROR;p->target=saved;p->face=savedFace;p->framebuffer();return ok;
}
bool GlesDevice::captureTarget(TextureId id,const std::wstring& file) {
    std::vector<uint8_t> pixels;uint32_t pitch=0;
    if(!readTarget(id,pixels,&pitch)) return false;
    auto* surface=SDL_CreateSurfaceFrom(targetWidth(id),targetHeight(id),SDL_PIXELFORMAT_BGRA32,pixels.data(),pitch);
    if(!surface) return false;
    const bool ok=SDL_SaveBMP(surface,std::filesystem::path(file).string().c_str());SDL_DestroySurface(surface);return ok;
}
bool GlesDevice::captureFloatTarget(TextureId id,const std::wstring& file) { return captureTarget(id,file); }
bool GlesDevice::copyCubeFace(TextureId from,TextureId to,uint32_t face) {
    if(!from || !to || face>=6 || !p->textures.contains(from) || !p->textures.contains(to)) return false;
    if(!p->textures.at(from).cube || !p->textures.at(to).cube) return false;
    const auto saved=p->target,savedFace=p->face;
    p->target=from;p->face=face;p->framebuffer();GLint source=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&source);
    p->target=to;p->face=face;p->framebuffer();glBindFramebuffer(GL_READ_FRAMEBUFFER,source);glDisable(GL_SCISSOR_TEST);
    glBlitFramebuffer(0,0,targetWidth(from),targetHeight(from),0,0,targetWidth(to),targetHeight(to),GL_COLOR_BUFFER_BIT,GL_LINEAR);
    const bool ok=glGetError()==GL_NO_ERROR;p->target=saved;p->face=savedFace;p->framebuffer();return ok;
}
bool GlesDevice::captureCubeFace(TextureId id,uint32_t face,const std::wstring& file) {
    if(!id || !p->textures.contains(id) || !p->textures.at(id).cube || face>=6) return false;
    auto state=saveNativeState();const auto copy=createRenderTarget(targetWidth(id),targetHeight(id),false);
    if(!copy) return false;
    const bool ok=copyNativeTarget(id,face,{0,0,targetWidth(id),targetHeight(id),0,1},copy)&&captureTarget(copy,file);
    destroyTexture(copy);return ok;
}
void GlesDevice::destroyShader(ShaderId id) {
    if(std::this_thread::get_id()!=owner) return onOwner([&]{destroyShader(id);});
    if(!p->shaders.contains(id)) return;
    forgetNativeShader(id);
    for(auto [key,program]:p->programs) glDeleteProgram(program);
    p->programs.clear();p->shaders.erase(id);
    if(p->vs==id) p->vs=0;if(p->ps==id) p->ps=0;
}
void GlesDevice::setShadowMaps(const TextureId* maps,uint32_t count) {
    for(uint32_t i=0;i<6;++i) {
        setTexture(8+i,i<count&&maps?maps[i]:0);
        for(auto axis:{SamplerState::AddressU,SamplerState::AddressV}) setSamplerState(8+i,axis,3);
        setSamplerState(8+i,SamplerState::MinFilter,1);setSamplerState(8+i,SamplerState::MagFilter,1);setSamplerState(8+i,SamplerState::MipFilter,0);
    }
}
void GlesDevice::setSunShadowMap(TextureId map) {
    setTexture(14,map);setSamplerState(14,SamplerState::AddressU,3);setSamplerState(14,SamplerState::AddressV,3);
    setSamplerState(14,SamplerState::MinFilter,1);setSamplerState(14,SamplerState::MagFilter,1);setSamplerState(14,SamplerState::MipFilter,0);
}
QualityCaps GlesDevice::qualityCaps() const {
    QualityCaps caps;caps.smaa=true;
    if(SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
        GLfloat value=1;glGetFloatv(0x84ff,&value);caps.maxAnisotropy=uint32_t(value);
    }
    return caps;
}
bool GlesDevice::hasStencilBuffer() const { return true; }
void GlesDevice::setTextureFilter(uint32_t value) { p->textureFilter=std::min(value,5u); }
}
