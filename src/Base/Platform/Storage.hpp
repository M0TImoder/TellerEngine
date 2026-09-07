#pragma once

#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Base/Files.hpp>

#include <filesystem>
#include <utility>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

// EM_ASMの引数は$0の形で書く
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
#endif

namespace TellerEngine::Base::Platform {

// セーブの置き場所
// ネイティブでは普通のフォルダ、WASMではブラウザの保存領域に載る
class Storage {
public:
    explicit Storage(std::filesystem::path root) : root_(std::move(root)) {}

    const std::filesystem::path &Root() const { return root_; }

    std::filesystem::path PathOf(const std::string &name) const { return root_ / name; }

    // 使えるようにする
    // WASMでは前回までの内容をここで読み込む
    Expected<void, Error> Mount() {
        const auto ready = Files::EnsureDirectory(root_);
        if (!ready) {
            return Unexpected<Error>(ready.error());
        }
#if defined(__EMSCRIPTEN__)
        const int mounted = EM_ASM_INT(
            {
                var path = UTF8ToString($0);
                try {
                    FS.mount(IDBFS, {}, path);
                } catch (error) {
                    return 0;
                }
                return 1;
            },
            root_.c_str());
        if (mounted == 0) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, root_.string()});
        }
        return Sync(true);
#else
        return {};
#endif
    }

    // 書いたものを確実に残す
    Expected<void, Error> Flush() {
#if defined(__EMSCRIPTEN__)
        return Sync(false);
#else
        return {};
#endif
    }

private:
#if defined(__EMSCRIPTEN__)
    // 保存領域との往復が終わるまで待つ
    Expected<void, Error> Sync(bool load) {
        EM_ASM(
            {
                Module.tellerStorageState = 0;
                FS.syncfs($0 !== 0, function(error) {
                    Module.tellerStorageState = error ? 2 : 1;
                });
            },
            load ? 1 : 0);

        int state = 0;
        while ((state = EM_ASM_INT({ return Module.tellerStorageState; })) == 0) {
            emscripten_sleep(1);
        }
        if (state == 2) {
            return Unexpected<Error>(Error{ErrorCode::Unavailable, root_.string()});
        }
        return {};
    }
#endif

    std::filesystem::path root_;
};

} // namespace TellerEngine::Base::Platform

#if defined(__EMSCRIPTEN__)
#pragma clang diagnostic pop
#endif
