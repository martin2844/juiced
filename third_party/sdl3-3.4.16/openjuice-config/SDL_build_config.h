/* OpenJuice: SDL's Windows configuration (include/build_config/SDL_build_config_windows.h) for the game controller
   support alone, the fixed build's controllers (README.OpenJuice.md): the joysticks (XInput, RawInput, HIDAPI,
   Windows.Gaming.Input, DirectInput, GameInput), the gamepads, the haptics (DirectInput) and the sensors; no video,
   audio, render, GPU or camera. */
#ifndef OPENJUICE_SDL_BUILD_CONFIG_H
#define OPENJUICE_SDL_BUILD_CONFIG_H

#include "../include/build_config/SDL_build_config_windows.h"

#define SDL_AUDIO_DISABLED 1
#define SDL_VIDEO_DISABLED 1
#define SDL_RENDER_DISABLED 1
#define SDL_GPU_DISABLED 1
#define SDL_CAMERA_DISABLED 1

#undef SDL_AUDIO_DRIVER_WASAPI
#undef SDL_AUDIO_DRIVER_DSOUND
#undef SDL_AUDIO_DRIVER_DISK
#undef SDL_AUDIO_DRIVER_DUMMY
/* SDL_VIDEO_DRIVER_DUMMY kept: the video core needs a driver (never started: no SDL_INIT_VIDEO) */
#undef SDL_VIDEO_DRIVER_OFFSCREEN
#undef SDL_VIDEO_DRIVER_WINDOWS
#undef SDL_VIDEO_RENDER_D3D
#undef SDL_VIDEO_RENDER_D3D11
#undef SDL_VIDEO_RENDER_D3D12
#undef SDL_VIDEO_OPENGL
#undef SDL_VIDEO_OPENGL_WGL
#undef SDL_VIDEO_RENDER_OGL
#undef SDL_VIDEO_RENDER_OGL_ES2
#undef SDL_VIDEO_OPENGL_ES2
#undef SDL_VIDEO_OPENGL_EGL
#undef SDL_VIDEO_VULKAN
#undef SDL_VIDEO_RENDER_VULKAN
#undef SDL_VIDEO_RENDER_GPU
#undef SDL_GPU_D3D11
#undef SDL_GPU_D3D12
#undef SDL_GPU_VULKAN
#undef SDL_CAMERA_DRIVER_MEDIAFOUNDATION
#undef SDL_CAMERA_DRIVER_DUMMY

/* No tray icons (the dummy tray in place of the Windows one). */
#define SDL_TRAY_DUMMY 1

#endif
