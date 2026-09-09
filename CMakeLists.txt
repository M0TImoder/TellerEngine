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
                                        src/Base/Platform/TrueType.cpp)
  add_library(TellerEngine::Platform ALIAS TellerEnginePlatform)
  target_link_libraries(TellerEnginePlatform PUBLIC TellerEngine::TellerEngine SDL3::SDL3
                                             PRIVATE stb::stb)
endif()

# 抽出ツール
add_executable(TellerExtract
  src/Extract/Main.cpp
  src/Extract/Image.cpp
  src/Extract/Writer.cpp
)
target_link_libraries(TellerExtract PRIVATE TellerEngine::TellerEngine stb::stb)

# 見本
if(TELLER_ENABLE_PLATFORM AND PROJECT_IS_TOP_LEVEL)
  add_subdirectory(samples/Empty)
endif()

# テスト
if(TELLER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
