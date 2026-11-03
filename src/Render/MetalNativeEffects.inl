namespace oj::render {
NativeRenderDevice::Target MetalDevice::nativeTarget() const { return {p->target,p->face,p->viewport}; }
NativeRenderDevice::SavedState MetalDevice::saveNativeState() {
    p->endPass();
    const auto target=p->target,face=p->face,vs=p->vs,ps=p->ps,decl=p->decl,stream=p->stream,
               indices=p->indices,stride=p->stride,fvf=p->fvf,material=p->materialClass;
    const auto states=p->states;const auto sampling=p->sampling;const auto stages=p->stages;const auto bound=p->bound;
    const auto viewport=p->viewport;const auto clip=p->clip;const bool clipped=p->clipped,nativePass=p->nativePass;
    std::array<float,1024> vertex{},pixel{};
    std::copy_n(p->vertexRegisters.floats.begin(),vertex.size(),vertex.begin());
    std::copy_n(p->pixelRegisters.floats.begin(),pixel.size(),pixel.begin());p->nativePass=true;
    return SavedState([this,target,face,vs,ps,decl,stream,indices,stride,fvf,material,states,sampling,stages,bound,
                       viewport,clip,clipped,nativePass,vertex,pixel] {
        p->endPass();p->target=target;p->face=face;p->vs=vs;p->ps=ps;p->decl=decl;p->stream=stream;
        p->indices=indices;p->stride=stride;p->fvf=fvf;p->materialClass=material;p->states=states;p->sampling=sampling;
        p->stages=stages;p->bound=bound;p->viewport=viewport;p->clip=clip;p->clipped=clipped;p->nativePass=nativePass;
        std::copy(vertex.begin(),vertex.end(),p->vertexRegisters.floats.begin());
        std::copy(pixel.begin(),pixel.end(),p->pixelRegisters.floats.begin());
    });
}
TextureId MetalDevice::createNativeTarget(uint32_t w,uint32_t h,TargetFormat format) {
    const std::lock_guard lock(p->deviceLock);
    if(!w || !h) return 0;
    Impl::Texture t;t.w=w;t.h=h;
    t.format=format==TargetFormat::Scalar?SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
             format==TargetFormat::Hdr?SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    SDL_GPUTextureCreateInfo ci{};ci.type=SDL_GPU_TEXTURETYPE_2D;ci.format=t.format;
    ci.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER|SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    ci.width=w;ci.height=h;ci.layer_count_or_depth=1;ci.num_levels=1;
    t.gpu=SDL_CreateGPUTexture(p->gpu,&ci);if(!t.gpu) return 0;
    t.depth=p->makeDepth(w,h);const auto id=p->next++;p->textures.emplace(id,std::move(t));return id;
}
TextureId MetalDevice::createNativeScalarTexture(uint32_t w,uint32_t h,const float* values) {
    const std::lock_guard lock(p->deviceLock);
    Impl::Texture t;t.w=w;t.h=h;t.format=SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
    SDL_GPUTextureCreateInfo ci{};ci.type=SDL_GPU_TEXTURETYPE_2D;ci.format=t.format;ci.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width=w;ci.height=h;ci.layer_count_or_depth=1;ci.num_levels=1;
    t.gpu=SDL_CreateGPUTexture(p->gpu,&ci);if(!t.gpu) return 0;
    std::vector<uint16_t> half(size_t(w)*h);std::transform(values,values+half.size(),half.begin(),nativeHalf);
    p->uploadTexture(t,reinterpret_cast<const uint8_t*>(half.data()),half.size()*2,0,0,w,h);
    const auto id=p->next++;p->textures.emplace(id,std::move(t));return id;
}
TextureId MetalDevice::createNativeVolume(uint32_t w,uint32_t h,uint32_t depth,const float* rgba) {
    const std::lock_guard lock(p->deviceLock);
    Impl::Texture t;t.w=w;t.h=h;t.layers=depth;t.volume=true;t.format=SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    SDL_GPUTextureCreateInfo ci{};ci.type=SDL_GPU_TEXTURETYPE_3D;ci.format=t.format;ci.usage=SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ci.width=w;ci.height=h;ci.layer_count_or_depth=depth;ci.num_levels=1;
    t.gpu=SDL_CreateGPUTexture(p->gpu,&ci);if(!t.gpu) return 0;
    std::vector<uint16_t> half(size_t(w)*h*4);
    for(uint32_t z=0;z<depth;++z) {
        const float* slice=rgba+size_t(z)*w*h*4;
        std::transform(slice,slice+half.size(),half.begin(),nativeHalf);
        p->uploadTexture(t,reinterpret_cast<const uint8_t*>(half.data()),half.size()*2,0,z,w,h);
    }
    const auto id=p->next++;p->textures.emplace(id,std::move(t));return id;
}
bool MetalDevice::copyNativeTarget(TextureId from,uint32_t face,const Viewport& r,TextureId to) {
    const std::lock_guard lock(p->deviceLock);p->endPass();
    if(!to || !p->textures.contains(to) || !r.width || !r.height) return false;
    auto source=from?p->textures.at(from).gpu:p->backColor;if(!source) return false;
    SDL_GPUBlitInfo info{};info.source={source,0,face,r.x,r.y,r.width,r.height};
    const auto& target=p->textures.at(to);info.destination={target.gpu,0,0,0,0,target.w,target.h};
    info.load_op=SDL_GPU_LOADOP_DONT_CARE;info.filter=SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(p->commands(),&info);return true;
}
bool MetalDevice::copyNativeDepth(TextureId from,TextureId to) {
    const std::lock_guard lock(p->deviceLock);p->endPass();
    if(!to || !p->textures.contains(to)) return false;
    auto depth=from?p->textures.at(from).depth:p->backDepth;if(!depth) return false;
    auto& target=p->textures.at(to);
    if(target.w!=targetWidth(from) || target.h!=targetHeight(from)) return false;
    if(target.depth && target.ownsDepth) SDL_ReleaseGPUTexture(p->gpu,target.depth);
    target.depth=depth;target.ownsDepth=false;return true;
}
bool MetalDevice::captureTarget(TextureId id,const std::wstring& file) {
    std::vector<uint8_t> pixels;uint32_t pitch=0;
    if(!readTarget(id,pixels,&pitch)) return false;
    auto* surface=SDL_CreateSurfaceFrom(targetWidth(id),targetHeight(id),SDL_PIXELFORMAT_BGRA32,pixels.data(),pitch);
    if(!surface) return false;
    const bool ok=SDL_SaveBMP(surface,std::filesystem::path(file).c_str());SDL_DestroySurface(surface);return ok;
}
bool MetalDevice::captureFloatTarget(TextureId id,const std::wstring& file) { return captureTarget(id,file); }
bool MetalDevice::copyCubeFace(TextureId from,TextureId to,uint32_t face) {
    const std::lock_guard lock(p->deviceLock);p->endPass();
    if(!from || !to || face>=6 || !p->textures.contains(from) || !p->textures.contains(to)) return false;
    const auto& source=p->textures.at(from);const auto& target=p->textures.at(to);
    if(source.layers!=6 || target.layers!=6) return false;
    SDL_GPUBlitInfo info{};info.source={source.gpu,0,face,0,0,source.w,source.h};
    info.destination={target.gpu,0,face,0,0,target.w,target.h};info.load_op=SDL_GPU_LOADOP_DONT_CARE;info.filter=SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(p->commands(),&info);return true;
}
bool MetalDevice::captureCubeFace(TextureId id,uint32_t face,const std::wstring& file) {
    if(!id || !p->textures.contains(id) || p->textures.at(id).layers!=6 || face>=6) return false;
    auto state=saveNativeState();const auto copy=createRenderTarget(targetWidth(id),targetHeight(id),false);
    if(!copy) return false;
    const bool ok=copyNativeTarget(id,face,{0,0,targetWidth(id),targetHeight(id),0,1},copy)&&captureTarget(copy,file);
    destroyTexture(copy);return ok;
}
void MetalDevice::destroyShader(ShaderId id) {
    const std::lock_guard lock(p->deviceLock);if(!p->shaders.contains(id)) return;
    forgetNativeShader(id);p->endPass();for(auto [key,pipeline]:p->pipelines) SDL_ReleaseGPUGraphicsPipeline(p->gpu,pipeline);
    p->pipelines.clear();p->shaders.erase(id);if(p->vs==id) p->vs=0;if(p->ps==id) p->ps=0;
}
void MetalDevice::setShadowMaps(const TextureId* maps,uint32_t count) {
    for(uint32_t i=0;i<6;++i) {
        setTexture(8+i,i<count&&maps?maps[i]:0);
        for(auto axis:{SamplerState::AddressU,SamplerState::AddressV}) setSamplerState(8+i,axis,3);
        setSamplerState(8+i,SamplerState::MinFilter,1);setSamplerState(8+i,SamplerState::MagFilter,1);setSamplerState(8+i,SamplerState::MipFilter,0);
    }
}
void MetalDevice::setSunShadowMap(TextureId map) {
    setTexture(14,map);setSamplerState(14,SamplerState::AddressU,3);setSamplerState(14,SamplerState::AddressV,3);
    setSamplerState(14,SamplerState::MinFilter,1);setSamplerState(14,SamplerState::MagFilter,1);setSamplerState(14,SamplerState::MipFilter,0);
}
QualityCaps MetalDevice::qualityCaps() const { QualityCaps caps;caps.smaa=true;caps.maxAnisotropy=16;return caps; }
bool MetalDevice::hasStencilBuffer() const { return true; }
void MetalDevice::setTextureFilter(uint32_t value) { p->textureFilter=std::min(value,5u); }
}
