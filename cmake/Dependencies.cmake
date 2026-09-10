# ソースキャッシュ
if(NOT DEFINED CPM_SOURCE_CACHE AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
  if(WIN32)
    set(CPM_SOURCE_CACHE "$ENV{LOCALAPPDATA}/CPM" CACHE PATH "")
  else()
    set(CPM_SOURCE_CACHE "$ENV{HOME}/.cache/CPM" CACHE PATH "")
  endif()
endif()

include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

CPMUsePackageLock(${PROJECT_SOURCE_DIR}/CPM.lock)

# std::expectedが使えない環境向けの実体
CPMGetPackage(tl-expected)

# stb
CPMGetPackage(stb)
if(stb_ADDED)
  add_library(stb INTERFACE)
  add_library(stb::stb ALIAS stb)
  target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})
endif()

# 定義ファイル
CPMGetPackage(tomlplusplus)

# ウィンドウ・入力・ゲームパッド
if(TELLER_ENABLE_PLATFORM)
  CPMGetPackage(SDL3)

  # 音声
  CPMGetPackage(miniaudio)
  if(miniaudio_ADDED)
    add_library(miniaudio INTERFACE)
    add_library(miniaudio::miniaudio ALIAS miniaudio)
    target_include_directories(miniaudio SYSTEM INTERFACE ${miniaudio_SOURCE_DIR})
  endif()

  # デバッグUI
  if(TELLER_ENABLE_DEBUG_UI)
    CPMGetPackage(imgui)
    if(imgui_ADDED)
      add_library(TellerImGui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
      )
      add_library(imgui::imgui ALIAS TellerImGui)
      target_include_directories(TellerImGui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends ${imgui_SOURCE_DIR}/misc/cpp)
      target_compile_features(TellerImGui PUBLIC cxx_std_20)
      target_link_libraries(TellerImGui PUBLIC SDL3::SDL3)
    endif()
  endif()
endif()

# doctest
if(TELLER_BUILD_TESTS)
  CPMGetPackage(doctest)
endif()
