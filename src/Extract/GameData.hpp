#pragma once

// 抽出結果の保持

#include <Base/Compat.hpp>
#include <Extract/Bytes.hpp>
#include <Base/Error.hpp>
#include <Extract/Backgrounds.hpp>
#include <Extract/Chunks.hpp>
#include <Extract/Code.hpp>
#include <Extract/Fonts.hpp>
#include <Extract/General.hpp>
#include <Extract/Misc.hpp>
#include <Extract/Objects.hpp>
#include <Extract/Paths.hpp>
#include <Extract/Rooms.hpp>
#include <Extract/Scripts.hpp>
#include <Extract/Sounds.hpp>
#include <Extract/Sprites.hpp>
#include <Extract/Strings.hpp>
#include <Extract/Textures.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace TellerEngine::Extract {

struct GameData {
    ChunkTable chunks;
    GeneralInfo general;
    StringTable strings;

    TextureTable textures;
    TextureRegionTable regions;
    SpriteTable sprites;
    BackgroundTable backgrounds;
    FontTable fonts;
    SoundTable sounds;
    AudioTable audio;
    PathTable paths;
    ScriptTable scripts;
    ObjectTable objects;
    RoomTable rooms;

    RawChunk options;
    Language language;
    CountedList extensions;
    CountedList audioGroups;
    CountedList globalInit;
    CountedList shaders;
    CountedList timelines;
    RawChunk dataFiles;

    CodeTable code;
    VariableTable variables;
    FunctionTable functions;

    std::unordered_map<std::string, std::size_t> spriteByName;
    std::unordered_map<std::string, std::size_t> backgroundByName;
    std::unordered_map<std::string, std::size_t> fontByName;
    std::unordered_map<std::string, std::size_t> soundByName;
    std::unordered_map<std::string, std::size_t> pathByName;
    std::unordered_map<std::string, std::size_t> scriptByName;
    std::unordered_map<std::string, std::size_t> objectByName;
    std::unordered_map<std::string, std::size_t> roomByName;

    const Sprite *FindSprite(std::string_view name) const {
        return Find(sprites.sprites, spriteByName, name);
    }
    const Background *FindBackground(std::string_view name) const {
        return Find(backgrounds.backgrounds, backgroundByName, name);
    }
    const Font *FindFont(std::string_view name) const {
        return Find(fonts.fonts, fontByName, name);
    }
    const Sound *FindSound(std::string_view name) const {
        return Find(sounds.sounds, soundByName, name);
    }
    const Path *FindPath(std::string_view name) const {
        return Find(paths.paths, pathByName, name);
    }
    const Script *FindScript(std::string_view name) const {
        return Find(scripts.scripts, scriptByName, name);
    }
    const Object *FindObject(std::string_view name) const {
        return Find(objects.objects, objectByName, name);
    }
    const Room *FindRoom(std::string_view name) const {
        return Find(rooms.rooms, roomByName, name);
    }

private:
    template <typename T>
    static const T *
    Find(const std::vector<T> &entries,
         const std::unordered_map<std::string, std::size_t> &index,
         std::string_view name) {
        const auto found = index.find(std::string(name));
        if (found == index.end()) {
            return nullptr;
        }
        return &entries[found->second];
    }
};

namespace GameDataDetail {

template <typename T>
inline void BuildIndex(const std::vector<T> &entries,
                       std::unordered_map<std::string, std::size_t> &index) {
    index.reserve(entries.size());
    for (std::size_t at = 0; at < entries.size(); ++at) {
        index.emplace(entries[at].name, at);
    }
}

inline TellerEngine::Base::Error MissingChunk(std::string_view name) {
    return TellerEngine::Base::Error{
        TellerEngine::Base::ErrorCode::Malformed,
        std::string(name) + " chunk is missing"};
}

} // namespace GameDataDetail

inline Expected<GameData, TellerEngine::Base::Error>
LoadGameData(const TellerEngine::Extract::Bytes &bytes,
             std::string_view name = "data.win") {
    GameData data;

    auto chunks = ReadChunkTable(bytes, name);
    if (!chunks) {
        return Unexpected<TellerEngine::Base::Error>(chunks.error());
    }
    data.chunks = std::move(*chunks);

    // 必須のチャンクが揃っているかを先に確かめる
    for (const auto *required :
         {"GEN8", "STRG", "TXTR", "TPAG", "SPRT", "BGND", "FONT", "SOND",
          "AUDO", "PATH", "SCPT", "OBJT", "ROOM", "CODE", "VARI", "FUNC"}) {
        if (data.chunks.Find(required) == nullptr) {
            return Unexpected<TellerEngine::Base::Error>(
                GameDataDetail::MissingChunk(required));
        }
    }

#define TELLER_READ(target, call)                                              \
    {                                                                          \
        auto result = call;                                                    \
        if (!result) {                                                         \
            return Unexpected<TellerEngine::Base::Error>(                \
                result.error());                                               \
        }                                                                      \
        data.target = std::move(*result);                                      \
    }

    TELLER_READ(strings,
                ReadStringTable(bytes, name, *data.chunks.Find("STRG")))
    TELLER_READ(general, ReadGeneralInfo(bytes, name, *data.chunks.Find("GEN8"),
                                         data.strings))
    TELLER_READ(textures,
                ReadTextureTable(bytes, name, *data.chunks.Find("TXTR")))
    TELLER_READ(regions,
                ReadTextureRegions(bytes, name, *data.chunks.Find("TPAG"),
                                   data.textures.pages.size()))
    TELLER_READ(sprites, ReadSprites(bytes, name, *data.chunks.Find("SPRT"),
                                     data.strings, data.regions))
    TELLER_READ(backgrounds,
                ReadBackgrounds(bytes, name, *data.chunks.Find("BGND"),
                                data.strings, data.regions))
    TELLER_READ(fonts, ReadFonts(bytes, name, *data.chunks.Find("FONT"),
                                 data.strings, data.regions))
    TELLER_READ(sounds, ReadSounds(bytes, name, *data.chunks.Find("SOND"),
                                   data.strings, data.general.bytecodeVersion))
    TELLER_READ(audio, ReadAudio(bytes, name, *data.chunks.Find("AUDO")))
    TELLER_READ(paths,
                ReadPaths(bytes, name, *data.chunks.Find("PATH"), data.strings))
    TELLER_READ(scripts, ReadScripts(bytes, name, *data.chunks.Find("SCPT"),
                                     data.strings))
    TELLER_READ(objects, ReadObjects(bytes, name, *data.chunks.Find("OBJT"),
                                     data.strings))
    TELLER_READ(rooms,
                ReadRooms(bytes, name, *data.chunks.Find("ROOM"), data.strings))
    TELLER_READ(code,
                ReadCode(bytes, name, *data.chunks.Find("CODE"), data.strings))
    TELLER_READ(variables, ReadVariables(bytes, name, *data.chunks.Find("VARI"),
                                         data.strings))
    TELLER_READ(options, ReadRawChunk(bytes, name, *data.chunks.Find("OPTN")))
    TELLER_READ(language, ReadLanguage(bytes, name, *data.chunks.Find("LANG"),
                                       data.strings))
    TELLER_READ(extensions,
                ReadCountedList(bytes, name, *data.chunks.Find("EXTN")))
    TELLER_READ(audioGroups,
                ReadCountedList(bytes, name, *data.chunks.Find("AGRP")))
    TELLER_READ(globalInit,
                ReadCountedList(bytes, name, *data.chunks.Find("GLOB")))
    TELLER_READ(shaders,
                ReadCountedList(bytes, name, *data.chunks.Find("SHDR")))
    TELLER_READ(timelines,
                ReadCountedList(bytes, name, *data.chunks.Find("TMLN")))
    TELLER_READ(dataFiles, ReadRawChunk(bytes, name, *data.chunks.Find("DAFL")))
    TELLER_READ(functions, ReadFunctions(bytes, name, *data.chunks.Find("FUNC"),
                                         data.strings))

#undef TELLER_READ

    GameDataDetail::BuildIndex(data.sprites.sprites, data.spriteByName);
    GameDataDetail::BuildIndex(data.backgrounds.backgrounds,
                               data.backgroundByName);
    GameDataDetail::BuildIndex(data.fonts.fonts, data.fontByName);
    GameDataDetail::BuildIndex(data.sounds.sounds, data.soundByName);
    GameDataDetail::BuildIndex(data.paths.paths, data.pathByName);
    GameDataDetail::BuildIndex(data.scripts.scripts, data.scriptByName);
    GameDataDetail::BuildIndex(data.objects.objects, data.objectByName);
    GameDataDetail::BuildIndex(data.rooms.rooms, data.roomByName);

    return data;
}

} // namespace TellerEngine::Extract
