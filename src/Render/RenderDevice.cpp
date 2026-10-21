#include "Render/RenderDevice.h"

#include "Render/D3D9/D3D9Device.h"

namespace oj::render {

const char* backendName(Backend backend) {
    switch (backend) {
    case Backend::D3D9: return "d3d9";
    }
    return "?";
}

std::unique_ptr<IRenderDevice> createDevice(Backend backend, const DeviceDesc& desc, std::string* error) {
    switch (backend) {
    case Backend::D3D9: return d3d9::createDevice(desc, error);
    }
    return nullptr;
}

}  // namespace oj::render
