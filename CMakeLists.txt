cmake_minimum_required(VERSION 3.28)

project(TellerEngine
  VERSION 0.0.1
  LANGUAGES CXX
)

# デバッグUIの既定値
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(TELLER_DEBUG_UI_DEFAULT ON)
else()
  set(TELLER_DEBUG_UI_DEFAULT OFF)
endif()

option(TELLER_BUILD_TESTS "テストをビルドする" ${PROJECT_IS_TOP_LEVEL})
option(TELLER_ENABLE_DEBUG_UI "デバッグUIを組み込む" ${TELLER_DEBUG_UI_DEFAULT})
option(TELLER_ENABLE_PLATFORM "プラットフォーム層を組み込む" ON)

# 動作確認用
file(GLOB teller_samples CONFIGURE_DEPENDS LIST_DIRECTORIES true
  ${CMAKE_CURRENT_SOURCE_DIR}/samples/*)

set(TELLER_SAMPLE_DIRS "")
foreach(dir IN LISTS teller_samples)
  if(EXISTS ${dir}/CMakeLists.txt)
    list(APPEND TELLER_SAMPLE_DIRS ${dir})
  endif()
endforeach()

if(TELLER_SAMPLE_DIRS)
  set(TELLER_SAMPLES_DEFAULT ${PROJECT_IS_TOP_LEVEL})
else()
  set(TELLER_SAMPLES_DEFAULT OFF)
endif()

option(TELLER_BUILD_SAMPLES "動作確認用のものをビルドする" ${TELLER_SAMPLES_DEFAULT})

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_EXTENSIONS OFF)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# コンパイラ設定
include(cmake/CompilerFlags.cmake)

# 依存
include(cmake/Dependencies.cmake)

# WASMのループ
include(cmake/Asyncify.cmake)

# 単一スレッドで組む
if(EMSCRIPTEN)
  string(FIND "${CMAKE_CXX_FLAGS}" "-pthread" teller_pthread_flag)
  if(NOT teller_pthread_flag EQUAL -1)
    message(FATAL_ERROR "単一スレッドで組む必要があります (-pthread が指定されています)")
  endif()
endif()

# ライブラリ本体
add_library(TellerEngine INTERFACE)
add_library(TellerEngine::TellerEngine ALIAS TellerEngine)

target_compile_features(TellerEngine INTERFACE cxx_std_20)

# インクルード
target_include_directories(TellerEngine INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_link_libraries(TellerEngine INTERFACE tl::expected tomlplusplus::tomlplusplus)

target_compile_definitions(TellerEngine INTERFACE TOML_EXCEPTIONS=0)

if(TELLER_ENABLE_DEBUG_UI)
  target_compile_definitions(TellerEngine INTERFACE TELLER_DEBUG_UI)
endif()

# プラットフォーム層
if(TELLER_ENABLE_PLATFORM)
  add_library(TellerEnginePlatform STATIC src/Base/Platform/Image.cpp
                                        src/Base/Platform/TrueType.cpp
                                        src/Base/Platform/AudioDevice.cpp
                                        src/Base/Platform/DebugUi.cpp
                                        src/Base/Platform/DebugUiRenderer.cpp
                                        src/Base/Platform/DebugPanels.cpp)
  add_library(TellerEngine::Platform ALIAS TellerEnginePlatform)
  target_link_libraries(TellerEnginePlatform PUBLIC TellerEngine::TellerEngine SDL3::SDL3
                                             PRIVATE stb::stb miniaudio::miniaudio)
  if(TELLER_ENABLE_DEBUG_UI)
    target_link_libraries(TellerEnginePlatform PUBLIC imgui::imgui)
  endif()
endif()

# 抽出ツール
add_executable(TellerExtract
  src/Extract/Main.cpp
  src/Extract/Image.cpp
  src/Extract/Writer.cpp
)
target_link_libraries(TellerExtract PRIVATE TellerEngine::TellerEngine stb::stb)

# 動作確認用
if(TELLER_BUILD_SAMPLES AND TELLER_ENABLE_PLATFORM)
  if(NOT TELLER_SAMPLE_DIRS)
    message(FATAL_ERROR "TELLER_BUILD_SAMPLES が入だが samples/ に何も無い")
  endif()
  foreach(dir IN LISTS TELLER_SAMPLE_DIRS)
    file(RELATIVE_PATH relative ${CMAKE_CURRENT_SOURCE_DIR} ${dir})
    add_subdirectory(${relative})
  endforeach()
endif()

# デバッグUIを切った成果物にImGuiが残っていないことを確かめる
# テストを組まないリリースでも `--target VerifyNoDebugUi` で走らせられる
if(NOT TELLER_ENABLE_DEBUG_UI AND PROJECT_IS_TOP_LEVEL)
  set(teller_verified TellerExtract)
  if(TELLER_ENABLE_PLATFORM)
    list(APPEND teller_verified TellerEnginePlatform)
  endif()

  set(teller_verify_commands "")
  foreach(target IN LISTS teller_verified)
    list(APPEND teller_verify_commands
      COMMAND ${CMAKE_COMMAND} -DTARGET=$<TARGET_FILE:${target}>
              -P ${PROJECT_SOURCE_DIR}/cmake/NoDebugUi.cmake)
  endforeach()

  add_custom_target(VerifyNoDebugUi ${teller_verify_commands}
    DEPENDS ${teller_verified}
    VERBATIM)
endif()

# テスト
if(TELLER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
