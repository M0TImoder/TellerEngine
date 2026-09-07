#include <doctest/doctest.h>

#if !defined(__EMSCRIPTEN__)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

// 単一スレッドを崩す道具
const std::vector<std::string> kForbidden = {
    "<thread>",    "<mutex>",      "<future>",   "<condition_variable>",
    "<barrier>",   "<latch>",      "<semaphore>", "<stop_token>",
    "std::thread", "std::jthread", "std::async", "std::mutex",
};

std::vector<std::filesystem::path> SourceFiles() {
    std::vector<std::filesystem::path> files;
    const std::filesystem::path root{TELLER_SOURCE_DIR};
    if (!std::filesystem::exists(root)) {
        return files;
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string extension = entry.path().extension().string();
        if (extension == ".hpp" || extension == ".cpp") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string Read(const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{stream},
                       std::istreambuf_iterator<char>{}};
}

} // namespace

TEST_CASE("ソースがスレッドの道具を持ち込んでいない") {
    const std::vector<std::filesystem::path> files = SourceFiles();
    REQUIRE_FALSE(files.empty());

    for (const std::filesystem::path &file : files) {
        const std::string content = Read(file);
        for (const std::string &token : kForbidden) {
            INFO(file.string() << " が " << token << " を持ち込んでいる");
            CHECK(content.find(token) == std::string::npos);
        }
    }
}

#endif

TEST_CASE("WASMでスレッドが有効になっていない") {
#if !defined(__EMSCRIPTEN__)
    MESSAGE("ネイティブでは対象外");
#elif defined(__EMSCRIPTEN_PTHREADS__)
    FAIL("スレッドが有効になっている");
#else
    CHECK(true);
#endif
}
