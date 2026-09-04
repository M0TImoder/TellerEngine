#include <Teller/Data/FileBytes.hpp>

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path MakeSample(const std::string &contents) {
    const auto directory =
        std::filesystem::temp_directory_path() / "TellerDataBytes";
    std::filesystem::create_directories(directory);
    std::ofstream stream(directory / "sample.bin", std::ios::binary);
    stream.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    return directory;
}

} // namespace

TEST_CASE("FileBytesは範囲を指定して読める") {
    const auto directory = MakeSample("0123456789");
    const Teller::Data::FileBytes bytes(directory);

    CHECK(bytes.Has("sample.bin"));

    const auto size = bytes.SizeOf("sample.bin");
    REQUIRE(size.has_value());
    CHECK(*size == 10);

    const auto part = bytes.Read("sample.bin", 3, 4);
    REQUIRE(part.has_value());
    REQUIRE(part->size() == 4);
    CHECK(static_cast<char>((*part)[0]) == '3');
    CHECK(static_cast<char>((*part)[3]) == '6');

    const auto whole = bytes.ReadAll("sample.bin");
    REQUIRE(whole.has_value());
    CHECK(whole->size() == 10);
}

TEST_CASE("FileBytesは失敗を種別と文脈で返す") {
    const auto directory = MakeSample("0123456789");
    const Teller::Data::FileBytes bytes(directory);

    CHECK_FALSE(bytes.Has("missing.bin"));

    const auto missing = bytes.ReadAll("missing.bin");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == Teller::Data::ErrorCode::NotFound);
    CHECK(missing.error().context.find("missing.bin") != std::string::npos);

    const auto beyond = bytes.Read("sample.bin", 8, 5);
    REQUIRE_FALSE(beyond.has_value());
    CHECK(beyond.error().code == Teller::Data::ErrorCode::OutOfRange);
    CHECK(beyond.error().context.find("total=10") != std::string::npos);
}
