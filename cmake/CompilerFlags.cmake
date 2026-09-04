# -ffast-mathの禁止
# IEEE754の再現性維持用
set(_teller_flag_vars CMAKE_CXX_FLAGS)
foreach(_config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  list(APPEND _teller_flag_vars CMAKE_CXX_FLAGS_${_config})
endforeach()

foreach(_var IN LISTS _teller_flag_vars)
  foreach(_bad IN ITEMS "-ffast-math" "-Ofast" "/fp:fast")
    string(FIND "${${_var}}" "${_bad}" _found)
    if(NOT _found EQUAL -1)
      message(FATAL_ERROR
        "${_bad} は指定できません (${_var})\n"
        "IEEE754の再現性が壊れ、TASと本家準拠の検証が環境間で一致しなくなります")
    endif()
  endforeach()
endforeach()

unset(_teller_flag_vars)
unset(_config)
unset(_var)
unset(_bad)
unset(_found)

# コンパイラキャッシュ
# 単独ビルドのときだけ設定し、親プロジェクトの指定を上書きしない
if(PROJECT_IS_TOP_LEVEL AND NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
  find_program(TELLER_COMPILER_CACHE NAMES ccache sccache)
  if(TELLER_COMPILER_CACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${TELLER_COMPILER_CACHE}")
    message(STATUS "コンパイラキャッシュ: ${TELLER_COMPILER_CACHE}")
  endif()
endif()

# 警告
# -Werrorはこのリポジトリ自身のビルドだけに適用し、Modには伝播させない
if(PROJECT_IS_TOP_LEVEL)
  add_library(TellerWarnings INTERFACE)
  if(MSVC)
    target_compile_options(TellerWarnings INTERFACE /W4 /WX)
  else()
    target_compile_options(TellerWarnings INTERFACE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()
