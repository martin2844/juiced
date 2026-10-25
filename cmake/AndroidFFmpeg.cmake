include(ExternalProject)
set(OJ_FFMPEG_ARCHIVE "https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz" CACHE STRING "Pinned FFmpeg source URL or local archive")
set(ffmpeg_install "${CMAKE_BINARY_DIR}/ffmpeg-install")
file(MAKE_DIRECTORY "${ffmpeg_install}/include")
find_program(OJ_MAKE make REQUIRED)
ExternalProject_Add(ffmpeg_native
  URL "${OJ_FFMPEG_ARCHIVE}"
  URL_HASH SHA256=733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  INSTALL_DIR "${ffmpeg_install}"
  CONFIGURE_COMMAND "${PROJECT_SOURCE_DIR}/tools/configure-android-ffmpeg.sh" <SOURCE_DIR> <INSTALL_DIR> "${CMAKE_ANDROID_NDK}" "${ANDROID_ABI}"
  BUILD_COMMAND "${OJ_MAKE}" -j8
  INSTALL_COMMAND "${OJ_MAKE}" install
  BUILD_BYPRODUCTS "${ffmpeg_install}/lib/libavformat.so" "${ffmpeg_install}/lib/libavcodec.so" "${ffmpeg_install}/lib/libswresample.so" "${ffmpeg_install}/lib/libavutil.so")
add_library(android_ffmpeg INTERFACE)
foreach(component avformat avcodec swresample avutil)
  add_library(ffmpeg_${component} SHARED IMPORTED GLOBAL)
  set_target_properties(ffmpeg_${component} PROPERTIES IMPORTED_LOCATION "${ffmpeg_install}/lib/lib${component}.so"
    INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_install}/include")
  add_dependencies(ffmpeg_${component} ffmpeg_native)
  target_link_libraries(android_ffmpeg INTERFACE ffmpeg_${component})
endforeach()
