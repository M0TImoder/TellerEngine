// data.win からアセットを取り出す
// 使い方: TellerExtract <Undertaleフォルダ または data.win> <抽出先>
// [--data-only]

#include <Extract/FileBytes.hpp>
#include <Extract/GameData.hpp>
#include <Extract/Writer.hpp>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

using namespace TellerEngine;

namespace {

// 全角を2桁として数えた表示幅
std::size_t DisplayWidth(std::string_view text) {
    std::size_t width = 0;
    for (std::size_t at = 0; at < text.size();) {
        const auto lead = static_cast<unsigned char>(text[at]);
        const std::size_t length =
            lead < 0x80 ? 1 : (lead < 0xe0 ? 2 : (lead < 0xf0 ? 3 : 4));
        width += length >= 3 ? 2 : 1;
        at += length;
    }
    return width;
}

// 端末に進捗の帯を出す
// 同じ行を上書きするので、流れていかない
void DrawProgress(std::string_view stage, std::size_t done, std::size_t total) {
    constexpr int Width = 32;
    const double ratio =
        total == 0 ? 1.0
                   : static_cast<double>(done) / static_cast<double>(total);
    const int filled = static_cast<int>(ratio * Width);

    std::string bar;
    for (int at = 0; at < Width; ++at) {
        bar += at < filled ? "#" : "-";
    }
    std::string label(stage);
    while (DisplayWidth(label) < 18) {
        label += ' ';
    }
    std::printf("\r  %s [%s] %5zu / %-5zu\033[K", label.c_str(), bar.c_str(),
                done, total);
    std::fflush(stdout);
}

void Usage() {
    std::printf("使い方: TellerExtract <入力> <抽出先> [--data-only]\n"
                "\n"
                "  入力        Undertaleフォルダ、または data.win そのもの\n"
                "  抽出先      取り出したアセットを置く場所\n"
                "  --data-only 隣の音声ファイルを取り込まない\n"
                "\n"
                "既定ではフォルダを渡すと、data.win "
                "と隣の音声ファイルの両方を取り込む\n"
                "data.win だけを渡した場合、外部の音声は取り込めない\n");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        Usage();
        return 1;
    }

    const std::filesystem::path input = argv[1];
    const std::filesystem::path output = argv[2];

    // 空の抽出先を受け取ると、現在地に大量のファイルを撒いてしまう
    if (output.empty()) {
        std::printf("抽出先が空です\n");
        return 1;
    }
    bool dataOnly = false;
    for (int at = 3; at < argc; ++at) {
        if (std::string(argv[at]) == "--data-only") {
            dataOnly = true;
        } else {
            Usage();
            return 1;
        }
    }

    std::error_code failure;
    std::filesystem::path directory;
    std::string dataName;

    if (std::filesystem::is_directory(input, failure)) {
        directory = input;
        dataName = "data.win";
    } else if (std::filesystem::is_regular_file(input, failure)) {
        directory = input.parent_path();
        dataName = input.filename().string();
        dataOnly = true;
    } else {
        std::printf("入力が見つかりません: %s\n", input.string().c_str());
        return 1;
    }

    if (!std::filesystem::is_regular_file(directory / dataName, failure)) {
        std::printf("data.win が見つかりません: %s\n",
                    (directory / dataName).string().c_str());
        return 1;
    }

    const Extract::FileBytes bytes(directory);
    std::printf("読み込み: %s\n", (directory / dataName).string().c_str());

    const auto game = Extract::LoadGameData(bytes, dataName);
    if (!game) {
        std::printf("解析に失敗しました: %s / %s\n",
                    Base::ToString(game.error().code),
                    game.error().context.c_str());
        return 1;
    }
    std::printf("  %s %u.%u.%u.%u  bytecode %u\n", game->general.name.c_str(),
                game->general.major, game->general.minor, game->general.release,
                game->general.build, game->general.bytecodeVersion);

    Extract::WriteOptions options;
    options.gameDirectory = directory;
    options.copyExternalSounds = !dataOnly;

    std::printf("書き出し: %s\n\n", output.string().c_str());
    options.onProgress = DrawProgress;
    const auto report =
        Extract::WriteAssets(bytes, dataName, *game, output, options);
    std::printf("\r\033[K");
    if (!report) {
        std::printf("書き出しに失敗しました: %s / %s\n",
                    Base::ToString(report.error().code),
                    report.error().context.c_str());
        return 1;
    }

    std::printf("\n完了\n"
                "  スプライトのコマ    %zu\n"
                "  当たり判定マスク    %zu\n"
                "  背景                %zu\n"
                "  フォント            %zu\n"
                "  音（data.win内）    %zu\n"
                "  音（外部ファイル）  %zu\n"
                "  オブジェクト        %zu\n"
                "  ルーム              %zu\n"
                "  定義ファイル        %zu\n",
                report->spriteFrames, report->spriteMasks, report->backgrounds,
                report->fonts, report->embeddedSounds, report->externalSounds,
                report->objects, report->rooms, report->definitions);
    if (dataOnly) {
        std::printf("\n外部の音声は取り込んでいません\n");
    }
    return 0;
}
