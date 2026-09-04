# 全ヘッダをincludeするだけのTUを生成
file(GLOB_RECURSE teller_headers CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/src/Teller/*.h
  ${PROJECT_SOURCE_DIR}/src/Vanilla/*.h
)

set(all_headers "")
foreach(header IN LISTS teller_headers)
  file(RELATIVE_PATH relative ${PROJECT_SOURCE_DIR}/src ${header})
  string(APPEND all_headers "#include <${relative}>\n")
endforeach()

set(all_headers_path ${CMAKE_CURRENT_BINARY_DIR}/AllHeaders.cpp)
file(WRITE ${all_headers_path} "${all_headers}")

add_executable(TellerTests
  Main.cpp
  Compat.cpp
  ${all_headers_path}
)

target_link_libraries(TellerTests PRIVATE Teller::Teller doctest::doctest TellerWarnings)

add_test(NAME TellerTests COMMAND TellerTests)
