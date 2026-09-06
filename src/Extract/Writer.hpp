#pragma once

// 解析したdata.winを書き出す

#include <Base/Compat.hpp>
#include <Base/Error.hpp>
#include <Extract/Bytes.hpp>
#include <Extract/GameData.hpp>

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <string>

namespace TellerEngine::Extract {

struct WriteReport {
    std::size_t spriteFrames = 0;
    std::size_t spriteMasks = 0;
    std::size_t backgrounds = 0;
    std::size_t fonts = 0;
    std::size_t embeddedSounds = 0;
    std::size_t externalSounds = 0;
    std::size_t rooms = 0;
    std::size_t objects = 0;
    std::size_t definitions = 0;
};

// 進捗状況を返す
using ProgressCallback = std::function<void(
    std::string_view stage, std::size_t done, std::size_t total)>;

struct WriteOptions {
    // Undertaleフォルダを渡された場合、oggファイルごと取り込む
    std::filesystem::path gameDirectory;
    bool copyExternalSounds = true;
    ProgressCallback onProgress;
};

Expected<WriteReport, Base::Error>
WriteAssets(const Bytes &bytes, std::string_view dataName, const GameData &game,
            const std::filesystem::path &out, const WriteOptions &options);

} // namespace TellerEngine::Extract
