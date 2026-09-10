# デバッグUIを切ったときIMGUIが排除されているか確認

if(NOT EXISTS "${TARGET}")
  message(FATAL_ERROR "調べる対象がない: ${TARGET}")
endif()

file(STRINGS "${TARGET}" found REGEX "ImGui|imgui" LIMIT_COUNT 5)

if(found)
  string(REPLACE ";" "\n  " shown "${found}")
  message(FATAL_ERROR "ImGuiが残っている:\n  ${shown}")
endif()

message(STATUS "ImGuiは1つも残っていない: ${TARGET}")
