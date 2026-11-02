// The D3D backend defines this in RenderDevice.cpp with its factory.
#if defined(OJ_LINUX) || defined(OJ_ANDROID) || defined(OJ_MACOS)
#include "Render/RenderDevice.h"
#include "Core/Base/LoadingThread.h"
namespace oj::render {
void IRenderDevice::letLoadingRun() { frameTurns().pause(); }
}
#endif
