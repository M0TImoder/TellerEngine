#include <Extract/Image.hpp>
#include <Extract/Writer.hpp>

#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace TellerEngine::Extract {
namespace {

std::string Escape(const std::string &text) {
    std::string out;
    out.reserve(text.size() + 2);
    for (const char c : text) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(c);
        }
    }
    return out;
}

std::string Quote(const std::string &text) {
    return "\"" + Escape(text) + "\"";
}

Expected<void, Base::Error> WriteText(const std::filesystem::path &path,
                                      const std::string &body) {
    std::error_code failure;
    std::filesystem::create_directories(path.parent_path(), failure);
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return Unexpected<Base::Error>(Base::Error{
            Base::ErrorCode::ReadFailed, "cannot write " + path.string()});
    }
    stream.write(body.data(), static_cast<std::streamsize>(body.size()));
    return {};
}

Expected<void, Base::Error> WriteBytes(const std::filesystem::path &path,
                                       Span<const std::byte> body) {
    std::error_code failure;
    std::filesystem::create_directories(path.parent_path(), failure);
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return Unexpected<Base::Error>(Base::Error{
            Base::ErrorCode::ReadFailed, "cannot write " + path.string()});
    }
    stream.write(reinterpret_cast<const char *>(body.data()),
                 static_cast<std::streamsize>(body.size()));
    return {};
}

Image MaskToImage(Span<const std::byte> mask, std::uint32_t width,
                  std::uint32_t height, std::uint64_t stride) {
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.assign(static_cast<std::size_t>(width) * height * 4, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto at = static_cast<std::size_t>(y * stride + x / 8);
            if (at >= mask.size()) {
                continue;
            }
            const auto bit =
                static_cast<std::uint8_t>(mask[at]) >> (7 - (x % 8)) & 1;
            if (bit != 0) {
                const auto to = (static_cast<std::size_t>(y) * width + x) * 4;
                image.pixels[to + 0] = 255;
                image.pixels[to + 1] = 255;
                image.pixels[to + 2] = 255;
                image.pixels[to + 3] = 255;
            }
        }
    }
    return image;
}

struct Entry {
    std::string name;
    std::string relative;
};

void Collect(const Extract::GameData &game,
             const std::map<std::string, std::string> &soundFiles,
             std::vector<Entry> &out) {
    for (const auto &s : game.sprites.sprites) {
        out.push_back({s.name, "Sprites/" + s.name + ".toml"});
    }
    for (const auto &b : game.backgrounds.backgrounds) {
        out.push_back({b.name, "Backgrounds/" + b.name + ".toml"});
    }
    for (const auto &f : game.fonts.fonts) {
        out.push_back({f.name, "Fonts/" + f.name + ".toml"});
    }
    for (const auto &s : game.sounds.sounds) {
        const auto found = soundFiles.find(s.name);
        out.push_back(
            {s.name, "Sounds/" + (found != soundFiles.end()
                                      ? found->second
                                      : (s.file.empty() ? s.name : s.file))});
    }
    for (const auto &o : game.objects.objects) {
        out.push_back({o.name, "Objects/" + o.name + ".toml"});
    }
    for (const auto &r : game.rooms.rooms) {
        out.push_back({r.name, "Rooms/" + r.name + ".toml"});
    }
    for (const auto &p : game.paths.paths) {
        out.push_back({p.name, "Paths.toml"});
    }
    for (const auto &s : game.scripts.scripts) {
        out.push_back({s.name, "Scripts.toml"});
    }
}

// ファイル形式を検証
inline std::string ResolveSoundFile(const std::string &declared,
                                    Span<const std::byte> contents) {
    std::filesystem::path path(declared);
    if (contents.size() >= 4) {
        const auto *head = reinterpret_cast<const char *>(contents.data());
        if (std::string(head, 4) == "RIFF") {
            path.replace_extension(".wav");
        } else if (std::string(head, 4) == "OggS") {
            path.replace_extension(".ogg");
        }
    }
    return path.string();
}

} // namespace

Expected<void, Base::Error>
WriteAssetHeader(const GameData &game,
                 const std::map<std::string, std::string> &soundFiles,
                 const std::filesystem::path &path) {
    std::vector<Entry> entries;
    Collect(game, soundFiles, entries);

    std::ostringstream out;
    out << "#pragma once\n\n"
        << "#include <Base/Assets.hpp>\n\n"
        << "namespace TellerEngine::Vanilla {\n\n"
        << "class Assets : public Base::Assets {\n"
        << "public:\n"
        << "    using Base::Assets::Assets;\n\n";
    for (const auto &entry : entries) {
        out << "    static constexpr Base::AssetLocation " << entry.name
            << "{\"" << entry.name << "\", \"" << entry.relative << "\"};\n";
    }
    out << "};\n\n} // namespace TellerEngine::Vanilla\n";
    return WriteText(path, out.str());
}

Expected<WriteReport, Base::Error>
WriteAssets(const Bytes &bytes, std::string_view dataName, const GameData &game,
            const std::filesystem::path &out, const WriteOptions &options) {
    WriteReport report;

    const auto progress = [&](std::string_view stage, std::size_t done,
                              std::size_t total) {
        if (options.onProgress) {
            options.onProgress(stage, done, total);
        }
    };

#define TRY(expr)                                                              \
    {                                                                          \
        auto result = (expr);                                                  \
        if (!result) {                                                         \
            return Unexpected<Base::Error>(result.error());                    \
        }                                                                      \
    }

    // テクスチャページを一度だけ復号して使い回す
    std::vector<Image> pages;
    pages.reserve(game.textures.pages.size());
    for (const auto &page : game.textures.pages) {
        progress("テクスチャページ", pages.size(), game.textures.pages.size());
        const auto png = ReadTexturePng(bytes, dataName, page);
        if (!png) {
            return Unexpected<Base::Error>(png.error());
        }
        auto image = DecodePng(Span<const std::byte>(*png));
        if (!image) {
            return Unexpected<Base::Error>(image.error());
        }
        pages.push_back(std::move(*image));
    }

    // 矩形を1枚の絵として取り出す
    const auto cutRegion =
        [&](const TextureRegion &region) -> Expected<Image, Base::Error> {
        auto cropped = CropImage(pages[static_cast<std::size_t>(region.page)],
                                 region.sourceX, region.sourceY,
                                 region.sourceWidth, region.sourceHeight);
        if (!cropped) {
            return Unexpected<Base::Error>(cropped.error());
        }
        auto placed = PlaceInBounds(*cropped, region);
        if (!placed) {
            return Unexpected<Base::Error>(placed.error());
        }
        ClearTransparentColor(*placed);
        return placed;
    };

    // ゲーム全体の設定
    {
        std::ostringstream body;
        body << "name = " << Quote(game.general.name) << "\n"
             << "display_name = " << Quote(game.general.displayName) << "\n"
             << "file_name = " << Quote(game.general.filename) << "\n"
             << "config = " << Quote(game.general.config) << "\n"
             << "bytecode_version = "
             << static_cast<int>(game.general.bytecodeVersion) << "\n"
             << "debugger_disabled = "
             << (game.general.debuggerDisabled ? "true" : "false") << "\n"
             << "version = "
             << Quote(std::to_string(game.general.major) + "." +
                      std::to_string(game.general.minor) + "." +
                      std::to_string(game.general.release) + "." +
                      std::to_string(game.general.build))
             << "\n"
             << "window_width = " << game.general.defaultWindowWidth << "\n"
             << "window_height = " << game.general.defaultWindowHeight << "\n"
             << "game_id = " << game.general.gameId << "\n"
             << "steam_app_id = " << game.general.steamAppId << "\n"
             << "info_flags = " << game.general.info << "\n"
             << "timestamp = " << game.general.timestamp << "\n";
        body << "room_order = [";
        for (std::size_t at = 0; at < game.general.roomOrder.size(); ++at) {
            body << (at == 0 ? "" : ", ")
                 << Quote(game.rooms.rooms[game.general.roomOrder[at]].name);
        }
        body << "]\n";
        TRY(WriteText(out / "Game.toml", body.str()))
        report.definitions++;
    }

    // スプライト
    progress("テクスチャページ", game.textures.pages.size(),
             game.textures.pages.size());
    std::size_t spriteAt = 0;
    for (const auto &sprite : game.sprites.sprites) {
        progress("スプライト", spriteAt++, game.sprites.sprites.size());
        const auto directory = out / "Sprites" / sprite.name;

        for (std::size_t frame = 0; frame < sprite.frames.size(); ++frame) {
            const auto *region = game.regions.Find(sprite.frames[frame]);
            if (region == nullptr) {
                return Unexpected<Base::Error>(
                    Base::Error{Base::ErrorCode::Malformed,
                                sprite.name + " frame is not in TPAG"});
            }
            auto image = cutRegion(*region);
            if (!image) {
                return Unexpected<Base::Error>(image.error());
            }
            TRY(WritePng(*image, directory / (std::to_string(frame) + ".png")))
            report.spriteFrames++;
        }

        for (std::uint32_t mask = 0; mask < sprite.maskCount; ++mask) {
            const auto body = ReadSpriteMask(bytes, dataName, sprite, mask);
            if (!body) {
                return Unexpected<Base::Error>(body.error());
            }
            const auto image =
                MaskToImage(Span<const std::byte>(*body), sprite.width,
                            sprite.height, sprite.maskStride);
            TRY(WritePng(image,
                         directory / ("Mask" + std::to_string(mask) + ".png")))
            report.spriteMasks++;
        }

        std::ostringstream body;
        body << "width = " << sprite.width << "\n"
             << "height = " << sprite.height << "\n"
             << "origin_x = " << sprite.originX << "\n"
             << "origin_y = " << sprite.originY << "\n"
             << "margin_left = " << sprite.marginLeft << "\n"
             << "margin_right = " << sprite.marginRight << "\n"
             << "margin_bottom = " << sprite.marginBottom << "\n"
             << "margin_top = " << sprite.marginTop << "\n"
             << "bounding_box_mode = " << sprite.boundingBoxMode << "\n"
             << "separate_masks = " << sprite.separateMasks << "\n"
             << "transparent = " << (sprite.transparent ? "true" : "false")
             << "\n"
             << "smooth = " << (sprite.smooth ? "true" : "false") << "\n"
             << "preload = " << (sprite.preload ? "true" : "false") << "\n"
             << "frame_count = " << sprite.frames.size() << "\n"
             << "mask_count = " << sprite.maskCount << "\n";
        TRY(WriteText(out / "Sprites" / (sprite.name + ".toml"), body.str()))
        report.definitions++;
    }

    // 背景
    progress("スプライト", game.sprites.sprites.size(),
             game.sprites.sprites.size());
    std::size_t backgroundAt = 0;
    for (const auto &background : game.backgrounds.backgrounds) {
        progress("背景", backgroundAt++, game.backgrounds.backgrounds.size());
        if (background.texture != 0) {
            const auto *region = game.regions.Find(background.texture);
            if (region != nullptr) {
                auto image = cutRegion(*region);
                if (!image) {
                    return Unexpected<Base::Error>(image.error());
                }
                TRY(WritePng(*image,
                             out / "Backgrounds" / (background.name + ".png")))
                report.backgrounds++;
            }
        }
        std::ostringstream body;
        body << "transparent = " << (background.transparent ? "true" : "false")
             << "\n"
             << "smooth = " << (background.smooth ? "true" : "false") << "\n"
             << "preload = " << (background.preload ? "true" : "false") << "\n";
        TRY(WriteText(out / "Backgrounds" / (background.name + ".toml"),
                      body.str()))
        report.definitions++;
    }

    // フォント
    progress("背景", game.backgrounds.backgrounds.size(),
             game.backgrounds.backgrounds.size());
    std::size_t fontAt = 0;
    for (const auto &font : game.fonts.fonts) {
        progress("フォント", fontAt++, game.fonts.fonts.size());
        if (font.texture != 0) {
            const auto *region = game.regions.Find(font.texture);
            if (region != nullptr) {
                auto image = cutRegion(*region);
                if (!image) {
                    return Unexpected<Base::Error>(image.error());
                }
                TRY(WritePng(*image, out / "Fonts" / (font.name + ".png")))
            }
        }

        std::ostringstream body;
        body << "display_name = " << Quote(font.displayName) << "\n"
             << "em_size = "
             << (font.emSizeIsFloat ? font.emSizeFloat
                                    : static_cast<float>(font.emSize))
             << "\n"
             << "bold = " << (font.bold ? "true" : "false") << "\n"
             << "italic = " << (font.italic ? "true" : "false") << "\n"
             << "range_start = " << font.rangeStart << "\n"
             << "range_end = " << font.rangeEnd << "\n"
             << "charset = " << static_cast<int>(font.charset) << "\n"
             << "anti_aliasing = " << static_cast<int>(font.antiAliasing)
             << "\n"
             << "scale_x = " << font.scaleX << "\n"
             << "scale_y = " << font.scaleY << "\n";
        for (const auto &glyph : font.glyphs) {
            body << "\n[[glyph]]\n"
                 << "character = " << glyph.character << "\n"
                 << "x = " << glyph.sourceX << "\n"
                 << "y = " << glyph.sourceY << "\n"
                 << "width = " << glyph.sourceWidth << "\n"
                 << "height = " << glyph.sourceHeight << "\n"
                 << "shift = " << glyph.shift << "\n"
                 << "offset = " << glyph.offset << "\n";
        }
        TRY(WriteText(out / "Fonts" / (font.name + ".toml"), body.str()))
        report.fonts++;
        report.definitions++;
    }

    // 音声
    std::map<std::string, std::string> soundFiles;
    {
        std::ostringstream index;
        std::size_t soundAt = 0;
        for (const auto &sound : game.sounds.sounds) {
            progress("音声", soundAt++, game.sounds.sounds.size());
            std::string file = sound.file.empty() ? sound.name : sound.file;

            // SONDが名乗る拡張子とAUDOの実体の食い違いの防止
            ByteBuffer embedded;
            if (sound.audio >= 0 &&
                static_cast<std::size_t>(sound.audio) <
                    game.audio.clips.size() &&
                sound.IsEmbedded()) {
                auto clip = ReadAudioClip(
                    bytes, dataName,
                    game.audio.clips[static_cast<std::size_t>(sound.audio)]);
                if (!clip) {
                    return Unexpected<Base::Error>(clip.error());
                }
                embedded = std::move(*clip);
                file = ResolveSoundFile(file, Span<const std::byte>(embedded));
            }
            soundFiles.emplace(sound.name, file);

            index << "[[sound]]\n"
                  << "name = " << Quote(sound.name) << "\n"
                  << "file = " << Quote(file) << "\n"
                  << "declared_file = " << Quote(sound.file) << "\n"
                  << "type = " << Quote(sound.type) << "\n"
                  << "volume = " << sound.volume << "\n"
                  << "pitch = " << sound.pitch << "\n"
                  << "embedded = " << (sound.IsEmbedded() ? "true" : "false")
                  << "\n\n";

            if (!embedded.empty()) {
                TRY(WriteBytes(out / "Sounds" / file,
                               Span<const std::byte>(embedded)))
                report.embeddedSounds++;
            } else if (options.copyExternalSounds &&
                       !options.gameDirectory.empty()) {
                const auto source = options.gameDirectory / file;
                std::error_code failure;
                if (std::filesystem::is_regular_file(source, failure)) {
                    std::filesystem::create_directories(out / "Sounds",
                                                        failure);
                    std::filesystem::copy_file(
                        source, out / "Sounds" / file,
                        std::filesystem::copy_options::overwrite_existing,
                        failure);
                    if (!failure) {
                        report.externalSounds++;
                    }
                }
            }
        }
        TRY(WriteText(out / "Sounds.toml", index.str()))
        report.definitions++;
    }

    // オブジェクト
    progress("音声", game.sounds.sounds.size(), game.sounds.sounds.size());
    std::size_t objectAt = 0;
    for (const auto &object : game.objects.objects) {
        progress("オブジェクト", objectAt++, game.objects.objects.size());
        std::ostringstream body;
        body << "sprite = "
             << (object.sprite >= 0 && static_cast<std::size_t>(object.sprite) <
                                           game.sprites.sprites.size()
                     ? Quote(
                           game.sprites
                               .sprites[static_cast<std::size_t>(object.sprite)]
                               .name)
                     : "\"\"")
             << "\n"
             << "parent = "
             << (object.parent >= 0 && static_cast<std::size_t>(object.parent) <
                                           game.objects.objects.size()
                     ? Quote(
                           game.objects
                               .objects[static_cast<std::size_t>(object.parent)]
                               .name)
                     : "\"\"")
             << "\n"
             << "depth = " << object.depth << "\n"
             << "visible = " << (object.visible ? "true" : "false") << "\n"
             << "solid = " << (object.solid ? "true" : "false") << "\n"
             << "persistent = " << (object.persistent ? "true" : "false")
             << "\n"
             << "uses_physics = " << (object.usesPhysics ? "true" : "false")
             << "\n";
        for (const auto &event : object.events) {
            body << "\n[[event]]\n"
                 << "kind = " << static_cast<std::uint32_t>(event.kind) << "\n"
                 << "subtype = " << event.subtype << "\n";
            for (const auto &action : event.actions) {
                body << "code = " << action.code << "\n";
            }
        }
        TRY(WriteText(out / "Objects" / (object.name + ".toml"), body.str()))
        report.objects++;
        report.definitions++;
    }

    // ルーム
    progress("オブジェクト", game.objects.objects.size(),
             game.objects.objects.size());
    std::size_t roomAt = 0;
    for (const auto &room : game.rooms.rooms) {
        progress("ルーム", roomAt++, game.rooms.rooms.size());
        std::ostringstream body;
        body << "caption = " << Quote(room.caption) << "\n"
             << "width = " << room.width << "\n"
             << "height = " << room.height << "\n"
             << "speed = " << room.speed << "\n"
             << "persistent = " << (room.persistent ? "true" : "false") << "\n"
             << "background_color = " << room.backgroundColor << "\n"
             << "draw_background_color = "
             << (room.drawBackgroundColor ? "true" : "false") << "\n"
             << "flags = " << room.flags << "\n";

        for (const auto &instance : room.instances) {
            body << "\n[[instance]]\n"
                 << "object = "
                 << (instance.object >= 0 &&
                             static_cast<std::size_t>(instance.object) <
                                 game.objects.objects.size()
                         ? Quote(game.objects
                                     .objects[static_cast<std::size_t>(
                                         instance.object)]
                                     .name)
                         : "\"\"")
                 << "\n"
                 << "id = " << instance.id << "\n"
                 << "x = " << instance.x << "\n"
                 << "y = " << instance.y << "\n"
                 << "scale_x = " << instance.scaleX << "\n"
                 << "scale_y = " << instance.scaleY << "\n"
                 << "rotation = " << instance.rotation << "\n"
                 << "color = " << instance.color << "\n";
        }

        for (const auto &tile : room.tiles) {
            body << "\n[[tile]]\n"
                 << "background = "
                 << (tile.background >= 0 &&
                             static_cast<std::size_t>(tile.background) <
                                 game.backgrounds.backgrounds.size()
                         ? Quote(game.backgrounds
                                     .backgrounds[static_cast<std::size_t>(
                                         tile.background)]
                                     .name)
                         : "\"\"")
                 << "\n"
                 << "id = " << tile.id << "\n"
                 << "x = " << tile.x << "\n"
                 << "y = " << tile.y << "\n"
                 << "source_x = " << tile.sourceX << "\n"
                 << "source_y = " << tile.sourceY << "\n"
                 << "width = " << tile.width << "\n"
                 << "height = " << tile.height << "\n"
                 << "depth = " << tile.depth << "\n"
                 << "scale_x = " << tile.scaleX << "\n"
                 << "scale_y = " << tile.scaleY << "\n";
        }

        for (const auto &view : room.views) {
            body << "\n[[view]]\n"
                 << "enabled = " << (view.enabled ? "true" : "false") << "\n"
                 << "view_x = " << view.viewX << "\n"
                 << "view_y = " << view.viewY << "\n"
                 << "view_width = " << view.viewWidth << "\n"
                 << "view_height = " << view.viewHeight << "\n"
                 << "port_x = " << view.portX << "\n"
                 << "port_y = " << view.portY << "\n"
                 << "port_width = " << view.portWidth << "\n"
                 << "port_height = " << view.portHeight << "\n"
                 << "border_x = " << view.borderX << "\n"
                 << "border_y = " << view.borderY << "\n"
                 << "speed_x = " << view.speedX << "\n"
                 << "speed_y = " << view.speedY << "\n";
        }
        TRY(WriteText(out / "Rooms" / (room.name + ".toml"), body.str()))
        report.rooms++;
        report.definitions++;
    }

    // 経路とスクリプト
    {
        std::ostringstream body;
        for (const auto &path : game.paths.paths) {
            body << "[[path]]\n"
                 << "name = " << Quote(path.name) << "\n"
                 << "smooth = " << (path.smooth ? "true" : "false") << "\n"
                 << "closed = " << (path.closed ? "true" : "false") << "\n"
                 << "precision = " << path.precision << "\n"
                 << "points = [";
            for (std::size_t at = 0; at < path.points.size(); ++at) {
                body << (at == 0 ? "" : ", ") << "[" << path.points[at].x
                     << ", " << path.points[at].y << ", "
                     << path.points[at].speed << "]";
            }
            body << "]\n\n";
        }
        TRY(WriteText(out / "Paths.toml", body.str()))
        report.definitions++;
    }
    {
        std::ostringstream body;
        for (const auto &script : game.scripts.scripts) {
            body << "[[script]]\nname = " << Quote(script.name)
                 << "\ncode = " << script.code << "\n\n";
        }
        TRY(WriteText(out / "Scripts.toml", body.str()))
        report.definitions++;
    }

    progress("ルーム", game.rooms.rooms.size(), game.rooms.rooms.size());
    progress("アセット一覧", 0, 1);
    TRY(WriteAssetHeader(game, soundFiles,
                         out / "Include" / "Vanilla" / "Assets.hpp"))
    report.definitions++;

    progress("アセット一覧", 1, 1);

#undef TRY
    return report;
}

} // namespace TellerEngine::Extract
