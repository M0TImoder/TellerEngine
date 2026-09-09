#include <Base/Files.hpp>
#include <Base/Platform/AudioDevice.hpp>
#include <Vanilla/MusicIndex.hpp>
#include <Vanilla/Sound.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;
namespace Vanilla = TellerEngine::Vanilla;

namespace {

const auto root = std::filesystem::temp_directory_path() / "teller-vanilla-sound";

constexpr std::uint32_t kRate = 44100;

void PutBytes(std::vector<std::byte> &out, const char *text, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(static_cast<std::byte>(text[i]));
    }
}

void PutNumber(std::vector<std::byte> &out, std::uint32_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xFF));
    }
}

std::filesystem::path MakeWave(const std::string &name, double seconds) {
    const auto frames = static_cast<std::uint32_t>(kRate * seconds);
    const std::uint32_t data = frames * 2;

    std::vector<std::byte> out;
    PutBytes(out, "RIFF", 4);
    PutNumber(out, 36 + data, 4);
    PutBytes(out, "WAVE", 4);
    PutBytes(out, "fmt ", 4);
    PutNumber(out, 16, 4);
    PutNumber(out, 1, 2);
    PutNumber(out, 1, 2);
    PutNumber(out, kRate, 4);
    PutNumber(out, kRate * 2, 4);
    PutNumber(out, 2, 2);
    PutNumber(out, 16, 2);
    PutBytes(out, "data", 4);
    PutNumber(out, data, 4);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const double wave = std::sin(2.0 * 3.14159265358979 * 440.0 * i / kRate);
        PutNumber(out, static_cast<std::uint32_t>(static_cast<std::int16_t>(wave * 16000.0)),
                  2);
    }

    const auto path = root / name;
    REQUIRE(Base::Files::EnsureDirectory(root).has_value());
    REQUIRE(Base::Files::WriteAtomic(path, TellerEngine::Span<const std::byte>{out})
                .has_value());
    return path;
}

Platform::AudioSettings Silent(std::size_t limit = 128) {
    Platform::AudioSettings settings;
    settings.silent = true;
    settings.voiceLimit = limit;
    return settings;
}

} // namespace

TEST_CASE("曲の道からアセットの名前を引ける") {
    CHECK(Vanilla::MusicAsset("music/story.ogg") == "mus_story");
    CHECK(Vanilla::MusicAsset("music/zz_megalovania.ogg") == "mus_zz_megalovania");
    CHECK(Vanilla::MusicAsset("music/drum/cuica.ogg") == "mus_drumcuica");
    CHECK(Vanilla::MusicAsset("music/paino/piano1.ogg") == "mus_piano1");
    CHECK(Vanilla::MusicAsset("music/sfx_woofenstein.ogg") == "mus_woofenstein");
    CHECK(Vanilla::MusicAsset("music/存在しない.ogg").empty());
    CHECK(Vanilla::MusicAsset("").empty());
}

TEST_CASE("対応表は名前の順に並んでいる") {
    CHECK(Vanilla::kMusicIndex.size() == 270);
    CHECK(std::is_sorted(Vanilla::kMusicIndex.begin(), Vanilla::kMusicIndex.end(),
                         [](const Vanilla::MusicEntry &left, const Vanilla::MusicEntry &right) {
                             return left.first < right.first;
                         }));
}

TEST_CASE("対応表はコンパイル時に引ける") {
    static_assert(Vanilla::MusicAsset("music/toriel.ogg") == "mus_toriel");
    static_assert(Vanilla::MusicAsset("music/無い.ogg").empty());
}

TEST_CASE("効果音は曲より先に消える") {
    auto audio = Platform::AudioDevice::Create(Silent(1));
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("priority.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId effect = Vanilla::PlaySound(*audio, *sound);
    CHECK(effect != Base::VoiceId::None);

    // 繰り返す曲の方が残りやすい
    const Base::VoiceId loop = Vanilla::LoopMusic(*audio, *sound, 1.0, 1.0);
    CHECK(loop != Base::VoiceId::None);
    CHECK_FALSE(audio->Playing(effect));

    // 効果音は割り込めない
    CHECK(Vanilla::PlaySound(*audio, *sound) == Base::VoiceId::None);
    CHECK(audio->Playing(loop));
}

TEST_CASE("曲の指定はアセットに残る") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("music.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = Vanilla::PlayMusic(*audio, *sound, 0.5, 0.75);
    CHECK(Vanilla::MusicVolume(*audio, *sound) == doctest::Approx(0.5));
    CHECK(Vanilla::MusicPitch(*audio, *sound) == doctest::Approx(0.75));
    CHECK(audio->Gain(voice) == doctest::Approx(0.5));
    CHECK(audio->Pitch(voice) == doctest::Approx(0.75));

    // 後から鳴らしたものにも効く
    const Base::VoiceId added = Vanilla::PlaySound(*audio, *sound);
    CHECK(audio->Gain(added) == doctest::Approx(0.5));
    CHECK(audio->Pitch(added) == doctest::Approx(0.75));
}

TEST_CASE("局所の指定はアセットに残らない") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("local.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = Vanilla::PlayMusicLocal(*audio, *sound, 0.8, 0.65);
    CHECK(audio->Gain(voice) == doctest::Approx(0.8));
    CHECK(audio->Pitch(voice) == doctest::Approx(0.65));

    CHECK(Vanilla::MusicVolume(*audio, *sound) == doctest::Approx(1.0));
    CHECK(Vanilla::MusicPitch(*audio, *sound) == doctest::Approx(1.0));

    const Base::VoiceId added = Vanilla::PlaySound(*audio, *sound);
    CHECK(audio->Gain(added) == doctest::Approx(1.0));
    CHECK(audio->Pitch(added) == doctest::Approx(1.0));
}

TEST_CASE("繰り返す曲は終わらない") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("loopmusic.wav", 0.1));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = Vanilla::LoopMusic(*audio, *sound, 1.0, 1.0);
    audio->Mix(static_cast<std::uint64_t>(audio->SampleRate() * 0.5));
    audio->Collect();
    CHECK(Vanilla::MusicPlaying(*audio, *sound));
    CHECK(audio->Playing(voice));
}

TEST_CASE("曲は止められる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto first = audio->Load(MakeWave("stop1.wav", 0.2));
    const auto second = audio->Load(MakeWave("stop2.wav", 0.2));
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    Vanilla::LoopMusic(*audio, *first, 1.0, 1.0);
    Vanilla::LoopMusic(*audio, *second, 1.0, 1.0);

    Vanilla::StopMusic(*audio, *first);
    CHECK_FALSE(Vanilla::MusicPlaying(*audio, *first));
    CHECK(Vanilla::MusicPlaying(*audio, *second));

    Vanilla::FreeAllMusic(*audio);
    CHECK_FALSE(Vanilla::MusicPlaying(*audio, *second));
    CHECK(audio->VoiceCount() == 0);
}

TEST_CASE("曲は止めずに休ませられる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("hold.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = Vanilla::LoopMusic(*audio, *sound, 1.0, 1.0);
    Vanilla::PauseMusic(*audio, voice);
    CHECK_FALSE(audio->Playing(voice));
    CHECK(audio->VoiceCount() == 1);

    Vanilla::ResumeMusic(*audio, voice);
    CHECK(audio->Playing(voice));
}

TEST_CASE("効果音は止められる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("effect.wav", 0.2));
    REQUIRE(sound.has_value());

    Vanilla::PlaySound(*audio, *sound);
    CHECK(Vanilla::SoundPlaying(*audio, *sound));

    Vanilla::StopSound(*audio, *sound);
    CHECK_FALSE(Vanilla::SoundPlaying(*audio, *sound));
}

TEST_CASE("音量と再生レートは後からも変えられる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("adjust.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = Vanilla::LoopMusic(*audio, *sound, 1.0, 1.0);
    Vanilla::SetMusicVolume(*audio, *sound, 0.3);
    Vanilla::SetMusicPitch(*audio, *sound, 1.5);

    CHECK(audio->Gain(voice) == doctest::Approx(0.3));
    CHECK(audio->Pitch(voice) == doctest::Approx(1.5));
    CHECK(Vanilla::MusicVolume(*audio, *sound) == doctest::Approx(0.3));
    CHECK(Vanilla::MusicPitch(*audio, *sound) == doctest::Approx(1.5));
}

TEST_CASE("定位の指定は何もしない") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("pan.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = Vanilla::LoopMusic(*audio, *sound, 1.0, 1.0);
    Vanilla::SetMusicPanning(*audio, *sound, -1.0);
    CHECK(audio->Playing(voice));
    CHECK(audio->Gain(voice) == doctest::Approx(1.0));
}
