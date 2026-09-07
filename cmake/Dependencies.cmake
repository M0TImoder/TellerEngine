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

# ウィンドウ・入力・ゲームパッド
if(TELLER_ENABLE_PLATFORM)
  CPMGetPackage(SDL3)
endif()

# doctest
if(TELLER_BUILD_TESTS)
  CPMGetPackage(doctest)
endif()
