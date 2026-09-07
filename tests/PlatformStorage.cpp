#include <Base/Files.hpp>
#include <Base/Platform/Storage.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <string>

namespace Files = TellerEngine::Base::Files;
namespace Platform = TellerEngine::Base::Platform;

namespace {

std::filesystem::path Scratch() {
#if defined(__EMSCRIPTEN__)
    return "/teller-storage";
#else
    return std::filesystem::temp_directory_path() / "teller-storage";
#endif
}

} // namespace

TEST_CASE("置き場所を用意して使える") {
#if defined(__EMSCRIPTEN__)
    // ブラウザの外には保存領域が無いので失敗する
    Platform::Storage storage{Scratch()};
    CHECK_FALSE(storage.Mount().has_value());
#else
    std::error_code ignored;
    std::filesystem::remove_all(Scratch(), ignored);

    Platform::Storage storage{Scratch()};
    REQUIRE(storage.Mount().has_value());
    CHECK(std::filesystem::is_directory(storage.Root()));

    const auto path = storage.PathOf("file0");
    REQUIRE(Files::WriteAtomicText(path, "20").has_value());
    REQUIRE(storage.Flush().has_value());

    const auto text = Files::ReadText(path);
    REQUIRE(text.has_value());
    CHECK(*text == "20");
#endif
}

TEST_CASE("名前から位置が決まる") {
    Platform::Storage storage{Scratch()};
    CHECK(storage.PathOf("undertale.ini") == Scratch() / "undertale.ini");
    CHECK(storage.Root() == Scratch());
}
