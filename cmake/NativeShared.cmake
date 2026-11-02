# Platform-neutral input algorithms from the pinned upstream submodule.
function(oj_native_shared target)
  set(shared "${PROJECT_SOURCE_DIR}/external/JuicedConfigExt/src/Shared")
  if(NOT EXISTS "${shared}/PadActions.cpp")
    message(FATAL_ERROR "Run git submodule update --init external/JuicedConfigExt before building")
  endif()
  target_sources(${target} PRIVATE "${shared}/PadActions.cpp" "${shared}/ControlsFile.cpp" "${shared}/ForceFeedbackScale.cpp")
  target_include_directories(${target} PUBLIC "${shared}")
endfunction()
