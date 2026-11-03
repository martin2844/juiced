# Run on macOS against a staged copy of the built Juiced.app, never a live app.
# cmake -DAPP=/absolute/staging/Juiced.app -P cmake/BundleMac.cmake
cmake_minimum_required(VERSION 3.24)
if(NOT CMAKE_HOST_APPLE OR NOT IS_ABSOLUTE "${APP}" OR
   NOT EXISTS "${APP}/Contents/MacOS/Juiced")
  message(FATAL_ERROR "APP must be an absolute path to a staged macOS Juiced.app")
endif()
include(BundleUtilities)
fixup_bundle("${APP}" "" "/opt/homebrew/lib")
# Dependency install names changed, so sign the bundled copies again.
file(GLOB_RECURSE libraries "${APP}/Contents/Frameworks/*.dylib")
foreach(library IN LISTS libraries)
  execute_process(COMMAND /usr/bin/codesign --force --sign - "${library}"
    COMMAND_ERROR_IS_FATAL ANY)
endforeach()
execute_process(COMMAND /usr/bin/codesign --force --deep --sign - "${APP}"
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND /usr/bin/codesign --verify --deep --strict "${APP}"
  COMMAND_ERROR_IS_FATAL ANY)
verify_app("${APP}")
