#pragma once
// Direct3D 9 backend: the API of the stock game.

#include "Render/RenderDevice.h"

namespace oj::render::d3d9 {

std::unique_ptr<IRenderDevice> createDevice(const DeviceDesc& desc, std::string* error);

}  // namespace oj::render::d3d9
