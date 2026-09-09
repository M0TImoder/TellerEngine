# 全ヘッダをincludeするだけのTUを生成
file(GLOB_RECURSE teller_headers CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/src/Base/*.hpp
  ${PROJECT_SOURCE_DIR}/src/Vanilla/*.hpp
  ${PROJECT_SOURCE_DIR}/src/Teller/*.hpp
  ${PROJECT_SOURCE_DIR}/src/Extract/*.hpp
)

set(all_headers "")
foreach(header IN LISTS teller_headers)
  file(RELATIVE_PATH relative ${PROJECT_SOURCE_DIR}/src ${header})
  string(APPEND all_headers "#include <${relative}>\n")
endforeach()

set(all_headers_path ${CMAKE_CURRENT_BINARY_DIR}/AllHeaders.cpp)
file(WRITE ${all_headers_path} "${all_headers}")

# stbの実装が入る翻訳単位
add_library(TellerExtractImage STATIC ${PROJECT_SOURCE_DIR}/src/Extract/Image.cpp
                                   ${PROJECT_SOURCE_DIR}/src/Extract/Writer.cpp)
target_link_libraries(TellerExtractImage PUBLIC TellerEngine::TellerEngine stb::stb)

add_executable(TellerTests
  Main.cpp
  BaseActivation.cpp
  BaseAudio.cpp
  BaseBoundary.cpp
  BaseContext.cpp
  BaseDepth.cpp
  BaseDraw.cpp
  BaseFiles.cpp
  BaseFontInfo.cpp
  BaseGameObject.cpp
  BaseGlobals.cpp
  BaseInput.cpp
  BaseInstances.cpp
  BaseLoop.cpp
  BaseMath.cpp
  BasePresent.cpp
  BaseRandom.cpp
  BaseScheduler.cpp
  BaseState.cpp
  BaseText.cpp
  BaseTextDraw.cpp
  BaseTextLayout.cpp
  BaseTextTyper.cpp
  BaseTrueType.cpp
  BaseVariableCheck.cpp
  BaseVariables.cpp
  BaseUtf8.cpp
  BaseWith.cpp
  Compat.cpp
  DataBytes.cpp
  ExtractChunkReaders.cpp
  ExtractChunks.cpp
  ExtractGameData.cpp
  ExtractGeneral.cpp
  ExtractSprites.cpp
  ExtractStrings.cpp
  ExtractTextures.cpp
  PlatformGamepad.cpp
  PlatformLoop.cpp
  PlatformRenderer.cpp
  PlatformSingleThread.cpp
  PlatformStorage.cpp
  Literals.cpp
  TellerMarkup.cpp
  VanillaLegacyMarkup.cpp
  VanillaSound.cpp
  VanillaSprite.cpp
  PlatformSystem.cpp
  ${all_headers_path}
)

target_link_libraries(TellerTests PRIVATE TellerEngine::TellerEngine TellerEngine::Platform
                                    doctest::doctest TellerExtractImage TellerWarnings)

target_compile_definitions(TellerTests PRIVATE
  TELLER_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src")

add_test(NAME TellerTests COMMAND TellerTests)
