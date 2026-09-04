cmake_minimum_required(VERSION 3.28)

project(TellerEngine
    VERSION 0.0.1
    LANGUAGES CXX
)

# デバッグUIの既定値
if(EMSCRIPTEN)
    set(TELLER_DEBUG_UI_DEFAULT OFF)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(TELLER_DEBUG_UI_DEFAULT ON)
else()
    set(TELLER_DEBUG_UI_DEFAULT OFF)
endif()

option(TELLER_BUILD_TESTS "テストをビルドする" ${PROJECT_IS_TOP_LEVEL})
option(TELLER_ENABLE_DEBUG_UI "デバッグUIを組み込む" ${TELLER_DEBUG_UI_DEFAULT})

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# 依存
include(cmake/Dependencies.cmake)

# ライブラリ本体
add_library(Teller INTERFACE)
add_library(Teller::Teller ALIAS Teller)

target_compile_features(Teller INTERFACE cxx_std_20)

# インクルード
target_include_directories(Teller INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if(TELLER_ENABLE_DEBUG_UI)
    target_compile_definitions(Teller INTERFACE TELLER_DEBUG_UI)
endif()
