# Juiced draws the same surface with multiple shaders and depth-equality tests.
# Apple requires both invariant positions and preserveInvariance to keep those
# passes at exactly the same depth. SDL's pinned MSL loader uses options:nil.
# https://developer.apple.com/documentation/metal/mtlcompileoptions/preserveinvariance
set(_oj_metal_source "${sdl3_SOURCE_DIR}/src/gpu/metal/SDL_gpu_metal.m")
file(READ "${_oj_metal_source}" _oj_metal)
if(NOT _oj_metal MATCHES "OpenJuice: preserve multipass depth")
  set(_oj_before "        library = [renderer->device\n            newLibraryWithSource:codeString\n                         options:nil\n                           error:&error];")
  set(_oj_after "        // OpenJuice: preserve multipass depth across translated D3D shaders.\n        MTLCompileOptions *ojOptions = [[MTLCompileOptions alloc] init];\n        ojOptions.fastMathEnabled = NO;\n        ojOptions.preserveInvariance = YES;\n        library = [renderer->device\n            newLibraryWithSource:codeString\n                         options:ojOptions\n                           error:&error];\n#if !__has_feature(objc_arc)\n        [ojOptions release];\n#endif")
  string(FIND "${_oj_metal}" "${_oj_before}" _oj_at)
  if(_oj_at EQUAL -1)
    message(FATAL_ERROR "Pinned SDL Metal compiler changed; review the invariance patch")
  endif()
  string(REPLACE "${_oj_before}" "${_oj_after}" _oj_metal "${_oj_metal}")
  file(WRITE "${_oj_metal_source}" "${_oj_metal}")
endif()
