# Linux uses the current engine/SDL keyboard port and the GLES branch's desktop GL renderer.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(SDL3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Lua 5.4 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample)
pkg_check_modules(PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
include(FetchContent)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
set(CMAKE_DISABLE_FIND_PACKAGE_SDL2 ON CACHE BOOL "" FORCE)
set(PROFILE_GLSLES3 ON CACHE BOOL "" FORCE)
FetchContent_Declare(mojoshader URL https://github.com/icculus/mojoshader/archive/ad5dff84830c2863c841f4b1f4e3df78c705b383.tar.gz DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(mojoshader)
target_compile_definitions(mojoshader PRIVATE USE_SDL3)
target_link_libraries(mojoshader PUBLIC SDL3::SDL3)
file(GLOB_RECURSE CORE CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/Core/*.cpp)
list(FILTER CORE EXCLUDE REGEX "/(CpuInfo|Cpu|Config|Text|Log|GameSpyVoice|VoiceSound)\\.cpp$")
list(FILTER CORE EXCLUDE REGEX "/GameSpy/(DirectSound|OpenAL)/")
add_library(juice_core STATIC ${CORE} src/Mac/Base.cpp src/Mac/Compat/Windows.cpp src/Mac/VoiceUnavailable.cpp)
target_include_directories(juice_core PUBLIC src src/Mac/Compat)
include(${PROJECT_SOURCE_DIR}/cmake/NativeShared.cmake)
oj_native_shared(juice_core)
target_compile_options(juice_core PUBLIC -include ${PROJECT_SOURCE_DIR}/src/Mac/Compat/Crt.h)
target_compile_definitions(juice_core PUBLIC _DEBUG OJ_LINUX OJ_SDL_INPUT)
target_link_libraries(juice_core PUBLIC SDL3::SDL3)
file(GLOB_RECURSE RENDER CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/Render/*.cpp)
list(FILTER RENDER EXCLUDE REGEX "D3D9/|RenderDevice\\.cpp$|HlslCompiler\\.cpp$")
add_library(juice_render STATIC ${RENDER} src/Linux/Render/GlesDevice.cpp src/Linux/Render/GlShader.cpp)
target_include_directories(juice_render PRIVATE ${mojoshader_SOURCE_DIR})
target_link_libraries(juice_render PUBLIC juice_core mojoshader OpenGL::GL)
add_library(juice_audio STATIC src/Audio/Bank.cpp src/Audio/Player.cpp src/Audio/Voices.cpp src/Audio/SoundSystem.cpp src/Audio/Stream.cpp src/Mac/Audio.cpp src/Mac/Streams.cpp)
target_link_libraries(juice_audio PUBLIC juice_core PkgConfig::FFMPEG)
file(GLOB_RECURSE GAME CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/Game/*.cpp)
list(FILTER GAME EXCLUDE REGEX "App/(Main|CrashLog)\\.cpp$|Platform/(Platform|InputDevices|Settings|SystemKeys|Win7Imports|Windows7|Pads|WheelInput)\\.cpp$|Media/Binkw32\\.cpp$")
add_library(juice_game STATIC ${GAME} src/Mac/Platform.cpp src/Mac/Controls.cpp src/Mac/ControlsOverlay.cpp src/Mods/Packages.cpp src/Mods/Runtime.cpp src/Mods/Creative.cpp src/Mods/CreativeMenu.cpp src/Linux/ImageImport.cpp)
target_include_directories(juice_game PRIVATE ${LUA_INCLUDE_DIR})
target_link_libraries(juice_game PUBLIC juice_render juice_audio ${LUA_LIBRARIES} PkgConfig::PIXBUF)
add_executable(juiced src/Linux/Main.cpp)
target_sources(juice_game PRIVATE src/Linux/InputDevices.cpp)
target_link_libraries(juiced PRIVATE juice_game)
install(TARGETS juiced RUNTIME DESTINATION bin)
install(FILES LICENSE.md DESTINATION share/doc/openjuice RENAME LICENSE-OpenJuice.md)
install(FILES docs/THIRD-PARTY-LINUX.md DESTINATION share/doc/openjuice)
install(DIRECTORY docs/licenses/ DESTINATION share/doc/openjuice/licenses)
include(CTest)
if(BUILD_TESTING)
  add_executable(juiced_ffmpeg_license_check tools/FFmpegLicenseCheck.cpp)
  target_link_libraries(juiced_ffmpeg_license_check PRIVATE PkgConfig::FFMPEG)
  add_test(NAME ffmpeg_lgpl_regression COMMAND juiced_ffmpeg_license_check)
  add_executable(juiced_controller_check tests/Linux/ControllerTest.cpp)
  target_link_libraries(juiced_controller_check PRIVATE juice_game)
  add_executable(juiced_pink_slip_check tests/Android/HostPinkSlipTest.cpp)
  target_include_directories(juiced_pink_slip_check PRIVATE tests)
  target_link_libraries(juiced_pink_slip_check PRIVATE juice_game)
  add_executable(juiced_poker_check tools/PokerCheck.cpp)
  target_include_directories(juiced_poker_check PRIVATE tests)
  target_link_libraries(juiced_poker_check PRIVATE juice_game)
  add_executable(juiced_gl_check tools/LinuxRenderCheck.cpp)
  add_executable(juiced_loading_queue_check tests/Android/LoadingQueueTest.cpp)
  target_link_libraries(juiced_loading_queue_check PRIVATE juice_render)
  target_link_libraries(juiced_gl_check PRIVATE juice_render)
  add_executable(juiced_car_render_check tools/LinuxCarRenderCheck.cpp)
  target_link_libraries(juiced_car_render_check PRIVATE juice_game)
  add_executable(juiced_car_gallery tools/CarVisualGallery.cpp)
  target_link_libraries(juiced_car_gallery PRIVATE juice_game)
  add_executable(juiced_movie_check tests/Android/MovieLoadTest.cpp)
  target_link_libraries(juiced_movie_check PRIVATE juice_game)
  file(GLOB_RECURSE TESTS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/Unit/*.cpp)
  list(APPEND TESTS tests/Integration/Cars/CarViewTests.cpp)
  list(APPEND TESTS tests/Integration/Cars/CarModelPartsTests.cpp
    tests/Integration/Audio/SoundListenerTests.cpp
    tests/Integration/Audio/VoiceManagerTests.cpp
    tests/Integration/Frontend/CutSceneTests.cpp
    tests/Integration/Frontend/OnlineCrewRaceSetupTests.cpp)
  list(APPEND TESTS
    tests/Integration/GameData/CareerTests.cpp
    tests/Integration/GameData/CarDefsTests.cpp
    tests/Integration/GameData/CarPhysicsTests.cpp
    tests/Integration/GameData/TracksTests.cpp
    tests/Integration/GameData/ReplayTests.cpp
    tests/Integration/GameData/MoviesTests.cpp
    tests/Integration/GameData/ModelFilesTests.cpp
    tests/Integration/GameData/EffectsTests.cpp
    tests/Integration/GameData/ExeTablesTests.cpp
    tests/Integration/GameData/EffectDefsTests.cpp
    tests/Integration/GameData/GroundMapTests.cpp
    tests/Integration/GameData/SceneObjectsTests.cpp
    tests/Integration/Frontend/CareerRaceResultsTests.cpp
    tests/Integration/Frontend/AudioOptionsTests.cpp
    tests/Integration/Frontend/CarStatsTests.cpp
    tests/Integration/Frontend/CharacterTests.cpp
    tests/Integration/Frontend/GarageFixesTests.cpp
    tests/Integration/Frontend/GraphicsOptionsTests.cpp
    tests/Integration/Frontend/InstantSelTypeTests.cpp
    tests/Integration/Frontend/InstantSetupTests.cpp
    tests/Integration/Frontend/MainMenuTests.cpp
    tests/Integration/Frontend/MenuPictureTests.cpp
    tests/Integration/Frontend/MessageWindowTests.cpp
    tests/Integration/Frontend/OnlineLoginTests.cpp
    tests/Integration/Frontend/ScreenShowTests.cpp
    tests/Integration/Frontend/StartupTests.cpp
    tests/Integration/Frontend/WindowStackTests.cpp
    tests/Integration/Frontend/HostMeetingTests.cpp
    tests/Integration/Frontend/RespectChangesTests.cpp
    tests/Integration/Network/TcpTests.cpp
    tests/Integration/Network/TransportTests.cpp
    tests/Integration/Network/SessionTests.cpp
    tests/Integration/Network/HttpTests.cpp
    tests/Integration/Network/LeaderboardTests.cpp
    tests/Integration/Network/MatchmakingTests.cpp
    tests/Integration/Network/NatNegotiationTests.cpp
    tests/Integration/Network/PeerchatServerTests.cpp
    tests/Integration/Network/PresenceTests.cpp
    tests/Integration/Network/ServerBrowsingTests.cpp
    tests/Integration/Race/CruiseTricksTests.cpp
    tests/Integration/Race/HudReplayTests.cpp
    tests/Integration/Race/PostEffectsSplitTests.cpp
    tests/Integration/Race/RaceCameraTests.cpp
    tests/Integration/Race/RaceCarRecordTests.cpp
    tests/Integration/Race/RaceFrontEndTests.cpp
    tests/Integration/Race/RaceHudTargetTests.cpp
    tests/Integration/Race/RaceIntroTests.cpp
    tests/Integration/Saves/SaveDeviceTests.cpp
    tests/Integration/Saves/ReplaysDeviceTests.cpp
    tests/Integration/Saves/UserSavesTests.cpp)
  list(FILTER TESTS EXCLUDE REGEX "DirectXCheckTests\\.cpp$|CpuInfoTests\\.cpp$|VoiceTests\\.cpp$|VoiceCallsTests\\.cpp$|WheelSettingsTests\\.cpp$|SystemCommandTests\\.cpp$")
  add_executable(juiced_tests ${TESTS} tests/Framework/TestMain.cpp tests/Framework/Fixtures.cpp src/Mac/TestMain.cpp)
  target_compile_definitions(juiced_tests PRIVATE JUICED_TEST_MODS="${PROJECT_SOURCE_DIR}/mods")
  target_include_directories(juiced_tests PRIVATE tests)
  target_link_libraries(juiced_tests PRIVATE juice_game)
  add_test(NAME core_regression COMMAND juiced_tests --unit)
  add_test(NAME save_device_regression COMMAND juiced_tests --integration --suite SaveDevice)
  add_test(NAME replay_device_regression COMMAND juiced_tests --integration --suite ReplaysDevice)
  add_test(NAME network_tcp_regression COMMAND juiced_tests --integration --suite OnlineTcp)
  add_test(NAME network_transport_regression COMMAND juiced_tests --integration --suite Transport)
  add_test(NAME network_session_regression COMMAND juiced_tests --integration --suite Session)
  set_tests_properties(network_transport_regression network_session_regression
    PROPERTIES RESOURCE_LOCK openjuice_loopback_ports TIMEOUT 60)
  set_tests_properties(network_tcp_regression PROPERTIES TIMEOUT 30)
  # These protocol tests use local stand-in servers, not retired/public services.
  foreach(suite IN ITEMS GameSpyHttp Leaderboards Matchmaking NatNegotiation Peerchat Presence ServerBrowsing)
    string(TOLOWER "${suite}" test_suffix)
    add_test(NAME "network_${test_suffix}_regression"
      COMMAND juiced_tests --integration --suite "${suite}")
    set_tests_properties("network_${test_suffix}_regression"
      PROPERTIES RESOURCE_LOCK openjuice_loopback_ports TIMEOUT 60)
  endforeach()
endif()

if(BUILD_TESTING)
  add_executable(juiced_audio_check tests/Linux/AudioMixTest.cpp)
  target_link_libraries(juiced_audio_check PRIVATE juice_game)
  target_link_options(juiced_audio_check PRIVATE
    -Wl,--wrap=SDL_OpenAudioDeviceStream -Wl,--wrap=SDL_ResumeAudioStreamDevice)
  add_test(NAME audio_mix_regression COMMAND juiced_audio_check)
  foreach(pair IN ITEMS "runtime;Runtime" "packages;Packages" "creative;Creative")
    list(GET pair 0 target)
    list(GET pair 1 source)
    add_executable(juiced_${target}_check tests/Mac/${source}Test.cpp)
    target_include_directories(juiced_${target}_check PRIVATE tests)
    target_compile_definitions(juiced_${target}_check PRIVATE JUICED_TEST_MODS="${PROJECT_SOURCE_DIR}/mods")
    target_link_libraries(juiced_${target}_check PRIVATE juice_game)
    add_test(NAME mods_${target} COMMAND juiced_${target}_check)
  endforeach()
  add_executable(juiced_car_handling_check tools/CarHandlingCheck.cpp)
  target_include_directories(juiced_car_handling_check PRIVATE tests)
  target_link_libraries(juiced_car_handling_check PRIVATE juice_game)
  add_executable(juiced_wave2_car_check tools/street-cars-wave2/NativeCheck.cpp)
  target_include_directories(juiced_wave2_car_check PRIVATE tests)
  target_link_libraries(juiced_wave2_car_check PRIVATE juice_game)
  add_executable(juiced_street_car_check tools/StreetCarCheck.cpp)
  target_include_directories(juiced_street_car_check PRIVATE tests)
  target_link_libraries(juiced_street_car_check PRIVATE juice_game)
  add_executable(juiced_car_mod_check tests/Mac/CarModTest.cpp)
  target_include_directories(juiced_car_mod_check PRIVATE tests)
  target_link_libraries(juiced_car_mod_check PRIVATE juice_game)
  add_executable(juiced_wheel_mod_check mods/wheels/tools/Check.cpp)
  target_include_directories(juiced_wheel_mod_check PRIVATE tests)
  target_link_libraries(juiced_wheel_mod_check PRIVATE juice_game)
endif()

if(BUILD_TESTING)
  add_executable(juiced_neon_picker_check tests/Mac/NeonPickerTest.cpp)
  target_include_directories(juiced_neon_picker_check PRIVATE tests)
  target_compile_definitions(juiced_neon_picker_check PRIVATE JUICED_TEST_MODS="${PROJECT_SOURCE_DIR}/mods")
  target_link_libraries(juiced_neon_picker_check PRIVATE juice_game)
endif()

if(BUILD_TESTING AND NOT ANDROID)
  add_executable(juiced_native_effects_check tools/NativeEffectsCheck.cpp)
  target_link_libraries(juiced_native_effects_check PRIVATE juice_render)
endif()
