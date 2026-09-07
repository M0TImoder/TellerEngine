#include <Base/Error.hpp>
#include <Base/Files.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Base = TellerEngine::Base;
namespace Files = TellerEngine::Base::Files;

namespace {

std::filesystem::path Scratch() {
    const auto root = std::filesystem::temp_directory_path() / "teller-files";
    std::error_code ignored;
    std::filesystem::create_directories(root, ignored);
    return root;
}

TellerEngine::Span<const std::byte> BytesOf(const std::string &text) {
    return TellerEngine::Span<const std::byte>{
        reinterpret_cast<const std::byte *>(text.data()), text.size()};
}

} // namespace

TEST_CASE("書いたものが読める") {
    const auto path = Scratch() / "hello.txt";
    Files::Remove(path);

    REQUIRE(Files::WriteAtomicText(path, "こんにちは").has_value());
    CHECK(Files::Exists(path));

    const auto text = Files::ReadText(path);
    REQUIRE(text.has_value());
    CHECK(*text == "こんにちは");
}

TEST_CASE("バイト列のまま往復できる") {
    const auto path = Scratch() / "bytes.bin";
    const std::string source{"\x00\x01\xfe\xff", 4};

    REQUIRE(Files::WriteAtomic(path, BytesOf(source)).has_value());
    const auto bytes = Files::ReadBytes(path);
    REQUIRE(bytes.has_value());
    REQUIRE(bytes->size() == 4);
    CHECK(static_cast<unsigned char>((*bytes)[3]) == 0xff);
}

TEST_CASE("空のファイルも扱える") {
    const auto path = Scratch() / "empty.bin";
    REQUIRE(Files::WriteAtomicText(path, "").has_value());

    const auto bytes = Files::ReadBytes(path);
    REQUIRE(bytes.has_value());
    CHECK(bytes->empty());
}

TEST_CASE("無いファイルを読むとNotFoundになる") {
    const auto path = Scratch() / "居ない.txt";
    Files::Remove(path);

    const auto bytes = Files::ReadBytes(path);
    REQUIRE_FALSE(bytes.has_value());
    CHECK(bytes.error().code == Base::ErrorCode::NotFound);
    CHECK_FALSE(Files::Exists(path));
}

TEST_CASE("上書きしても途中の姿が残らない") {
    const auto path = Scratch() / "overwrite.txt";
    REQUIRE(Files::WriteAtomicText(path, "古い内容").has_value());
    REQUIRE(Files::WriteAtomicText(path, "新しい").has_value());

    const auto text = Files::ReadText(path);
    REQUIRE(text.has_value());
    CHECK(*text == "新しい");
    CHECK_FALSE(Files::Exists(std::filesystem::path{path}.concat(".writing")));
}

TEST_CASE("親のフォルダが無ければ作る") {
    const auto path = Scratch() / "深い" / "階層" / "file.txt";
    std::error_code ignored;
    std::filesystem::remove_all(Scratch() / "深い", ignored);

    REQUIRE(Files::WriteAtomicText(path, "中身").has_value());
    CHECK(Files::Exists(path));
}

TEST_CASE("フォルダを作れる") {
    const auto path = Scratch() / "作られる";
    std::error_code ignored;
    std::filesystem::remove_all(path, ignored);

    REQUIRE(Files::EnsureDirectory(path).has_value());
    CHECK(std::filesystem::is_directory(path));

    // 既にあっても失敗しない
    CHECK(Files::EnsureDirectory(path).has_value());
}

TEST_CASE("消せる") {
    const auto path = Scratch() / "消える.txt";
    REQUIRE(Files::WriteAtomicText(path, "x").has_value());
    CHECK(Files::Remove(path));
    CHECK_FALSE(Files::Exists(path));
    CHECK_FALSE(Files::Remove(path));
}
