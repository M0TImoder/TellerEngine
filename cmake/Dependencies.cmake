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

# doctest
if(TELLER_BUILD_TESTS)
  CPMGetPackage(doctest)
endif()
