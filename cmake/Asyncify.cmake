function(teller_enable_asyncify target)
  if(NOT EMSCRIPTEN)
    return()
  endif()

  cmake_parse_arguments(ARG "" "" "EXTRA" ${ARGN})

  set(names
    main
    _ZN12TellerEngine4Base8Platform13LoopCycleCtrl6UpdateEv
    _ZN12TellerEngine4Base8Platform13LoopCycleCtrl4WaitEv
    _ZN12TellerEngine4Base8Platform7Storage4SyncEb
    _ZN12TellerEngine4Base8Platform7Storage5MountEv
    _ZN12TellerEngine4Base8Platform7Storage5FlushEv
  )
  list(APPEND names ${ARG_EXTRA})
  list(JOIN names "," only)

  target_link_options(${target} PRIVATE
    "-sASYNCIFY=1"
    "-sASYNCIFY_ONLY=${only}"
    "-sALLOW_MEMORY_GROWTH=1"
  )
endfunction()
