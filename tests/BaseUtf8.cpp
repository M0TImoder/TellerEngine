#include <Base/Utf8.hpp>

#include <doctest/doctest.h>

namespace Base = TellerEngine::Base;

TEST_CASE("ASCIIは1バイト") {
    const Base::Utf8Char letter = Base::DecodeUtf8("abc", 0);
    CHECK(letter.code == U'a');
    CHECK(letter.size == 1);
}

TEST_CASE("日本語は3バイト") {
    const Base::Utf8Char letter = Base::DecodeUtf8("あ", 0);
    CHECK(letter.code == U'あ');
    CHECK(letter.size == 3);
}

TEST_CASE("4バイトの文字も読める") {
    const Base::Utf8Char letter = Base::DecodeUtf8("\xF0\x9F\x92\x94", 0);
    CHECK(letter.code == 0x1F494);
    CHECK(letter.size == 4);
}

TEST_CASE("壊れた並びは1バイトの置換文字") {
    const Base::Utf8Char letter = Base::DecodeUtf8("\xE3\x81", 0);
    CHECK(letter.code == 0xFFFD);
    CHECK(letter.size == 1);

    const Base::Utf8Char lone = Base::DecodeUtf8("\x80", 0);
    CHECK(lone.code == 0xFFFD);
    CHECK(lone.size == 1);
}

TEST_CASE("末尾を越えると空") {
    const Base::Utf8Char letter = Base::DecodeUtf8("a", 1);
    CHECK(letter.size == 0);
}

TEST_CASE("文字数を数えられる") {
    CHECK(Base::CountUtf8("あいu") == 3);
    CHECK(Base::CountUtf8("") == 0);
}

TEST_CASE("コンパイル時にも読める") {
    static_assert(Base::DecodeUtf8("あ", 0).size == 3);
    static_assert(Base::CountUtf8("abc") == 3);
}
