#pragma once

// SONDチャンクの音の定義と、AUDOチャンクの音の実体
// SONDの実体は36バイト固定
// AUDOの実体は長さと本体で、次の実体は4バイト境界に揃う
// 最後の実体だけは詰め物なしでチャンクの末尾に届く

#include <Extract/Chunks.hpp>
#include <Extract/Reader.hpp>
#include <Extract/Strings.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TellerEngine::Extract {

inline constexpr std::uint64_t SoundSize = 36;

// 本体がAUDOに入っているかどうかを表す
inline constexpr std::uint32_t SoundEmbedded = 0x1;
inline constexpr std::uint32_t SoundCompressed = 0x2;
inline constexpr std::uint32_t SoundRegular = 0x64;

struct Sound {
    std::string name;
    std::uint32_t flags = 0;
    std::string type;
    std::string file;
    std::uint32_t effects = 0;
    float volume = 1.0f;
    float pitch = 0.0f;
    // Regularが立ち、かつbytecode 14以上のときだけaudioGroupが入る
    // そうでなければ同じ位置にpreloadが入る
    std::int32_t audioGroup = -1;
    bool preload = false;
    std::int32_t audio = -1;

    bool IsEmbedded() const {
        return (flags & (SoundEmbedded | SoundCompressed)) != 0;
    }
};

struct SoundTable {
    std::vector<Sound> sounds;
};

inline Expected<SoundTable, TellerEngine::Base::Error>
ReadSounds(const TellerEngine::Extract::FileBytes &bytes, std::string_view name,
           const Chunk &chunk, const StringTable &strings,
           std::uint8_t bytecodeVersion) {
    const auto contents = ReadChunk(bytes, name, chunk);
    if (!contents) {
        return Unexpected<TellerEngine::Base::Error>(contents.error());
    }
    const Span<const std::byte> view(*contents);

    const auto pointers = ReadPointerList(view, chunk, "SOND");
    if (!pointers) {
        return Unexpected<TellerEngine::Base::Error>(pointers.error());
    }

    SoundTable table;
    table.sounds.reserve(pointers->size());

    for (const auto pointer : *pointers) {
        if (!InChunk(chunk, pointer, SoundSize)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("SOND entry runs past the end of the chunk"));
        }
        const auto at = LocalOffset(chunk, pointer);

        Sound sound;
        const std::size_t fields[] = {0, 8, 12};
        std::string *targets[] = {&sound.name, &sound.type, &sound.file};
        const char *labels[] = {"SOND name", "SOND type", "SOND file"};
        for (std::size_t index = 0; index < 3; ++index) {
            auto text = ResolveString(
                strings, ReadU32(view, at + fields[index]), labels[index]);
            if (!text) {
                return Unexpected<TellerEngine::Base::Error>(
                    text.error());
            }
            *targets[index] = std::move(*text);
        }

        sound.flags = ReadU32(view, at + 4);
        sound.effects = ReadU32(view, at + 16);
        sound.volume = ReadF32(view, at + 20);
        sound.pitch = ReadF32(view, at + 24);
        if ((sound.flags & SoundRegular) != 0 && bytecodeVersion >= 14) {
            sound.audioGroup = ReadI32(view, at + 28);
        } else {
            sound.preload = ReadU32(view, at + 28) != 0;
        }
        sound.audio = ReadI32(view, at + 32);
        table.sounds.push_back(std::move(sound));
    }

    return table;
}

struct AudioClip {
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct AudioTable {
    std::vector<AudioClip> clips;
};

inline Expected<AudioTable, TellerEngine::Base::Error>
ReadAudio(const TellerEngine::Extract::FileBytes &bytes, std::string_view name,
          const Chunk &chunk) {
    const auto head = bytes.Read(name, chunk.offset, 4);
    if (!head) {
        return Unexpected<TellerEngine::Base::Error>(head.error());
    }
    const std::uint64_t count = ReadU32(Span<const std::byte>(*head), 0);
    if (4 + count * 4 > chunk.size) {
        return Unexpected<TellerEngine::Base::Error>(Malformed(
            "AUDO declares " + std::to_string(count) + " clips but is only " +
            std::to_string(chunk.size) + " bytes"));
    }

    const auto list = bytes.Read(name, chunk.offset + 4, count * 4);
    if (!list) {
        return Unexpected<TellerEngine::Base::Error>(list.error());
    }
    const Span<const std::byte> listView(*list);

    AudioTable table;
    table.clips.reserve(static_cast<std::size_t>(count));

    for (std::uint64_t index = 0; index < count; ++index) {
        const std::uint64_t pointer =
            ReadU32(listView, static_cast<std::size_t>(index * 4));
        if (!InChunk(chunk, pointer, 4)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("AUDO clip " + std::to_string(index) +
                          " points outside the chunk"));
        }

        const auto length = bytes.Read(name, pointer, 4);
        if (!length) {
            return Unexpected<TellerEngine::Base::Error>(length.error());
        }

        AudioClip clip;
        clip.size = ReadU32(Span<const std::byte>(*length), 0);
        clip.offset = pointer + 4;

        if (!InChunk(chunk, clip.offset, clip.size)) {
            return Unexpected<TellerEngine::Base::Error>(
                Malformed("AUDO clip " + std::to_string(index) +
                          " runs past the end of the chunk"));
        }
        table.clips.push_back(clip);
    }

    return table;
}

inline Expected<TellerEngine::Extract::ByteBuffer,
                TellerEngine::Base::Error>
ReadAudioClip(const TellerEngine::Extract::FileBytes &bytes,
              std::string_view name, const AudioClip &clip) {
    return bytes.Read(name, clip.offset, clip.size);
}

} // namespace TellerEngine::Extract
