#include <Base/Files.hpp>
#include <Base/Platform/AudioDevice.hpp>

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace Base = TellerEngine::Base;
namespace Platform = TellerEngine::Base::Platform;

namespace {

const auto root = std::filesystem::temp_directory_path() / "teller-audio";

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

// 決まった長さの正弦波を書き出す
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

// 装置を開かずに動かす
Platform::AudioSettings Silent(std::size_t limit = 128) {
    Platform::AudioSettings settings;
    settings.silent = true;
    settings.voiceLimit = limit;
    return settings;
}

std::filesystem::path AssetRoot() {
    const char *value = std::getenv("TELLER_ASSETS");
    return value == nullptr ? std::filesystem::path{} : std::filesystem::path{value};
}

} // namespace

TEST_CASE("装置を開ける") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    CHECK(audio->SoundCount() == 0);
    CHECK(audio->VoiceCount() == 0);
}

TEST_CASE("音を読むと長さが取れる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());

    const auto sound = audio->Load(MakeWave("half.wav", 0.5));
    REQUIRE(sound.has_value());
    CHECK(*sound != Base::SoundId::None);
    CHECK(audio->SoundCount() == 1);
    CHECK(audio->Length(*sound) == doctest::Approx(0.5).epsilon(0.01));
    CHECK(audio->Length(Base::SoundId::None) == doctest::Approx(0.0));
}

TEST_CASE("読めないファイルは失敗する") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    CHECK_FALSE(audio->Load("この名前の音は無い.wav").has_value());
    CHECK(audio->SoundCount() == 0);
}

TEST_CASE("鳴らすと声が増え、止めると減る") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("one.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    CHECK(voice != Base::VoiceId::None);
    CHECK(audio->VoiceCount() == 1);
    CHECK(audio->Playing(voice));
    CHECK(audio->Playing(*sound));

    audio->Stop(voice);
    CHECK(audio->VoiceCount() == 0);
    CHECK_FALSE(audio->Playing(voice));
    CHECK_FALSE(audio->Playing(*sound));
}

TEST_CASE("同じ音を重ねて鳴らせる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("two.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId first = audio->Play(*sound);
    const Base::VoiceId second = audio->Play(*sound);
    CHECK(first != second);
    CHECK(audio->VoiceCount() == 2);

    audio->Stop(*sound);
    CHECK(audio->VoiceCount() == 0);
}

TEST_CASE("止めた声のIDはもう引けない") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("gone.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    audio->Stop(voice);

    CHECK_FALSE(audio->Playing(voice));
    CHECK(audio->Position(voice) == doctest::Approx(0.0));
    audio->Stop(voice);
    audio->Pause(voice);
    audio->Resume(voice);
    audio->SetGain(voice, 0.5);
    audio->SetPitch(voice, 2.0);
    CHECK(audio->VoiceCount() == 0);
}

TEST_CASE("一時停止した声は残る") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("pause.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    audio->Pause(voice);
    CHECK_FALSE(audio->Playing(voice));
    CHECK(audio->VoiceCount() == 1);

    audio->Collect();
    CHECK(audio->VoiceCount() == 1);

    audio->Resume(voice);
    CHECK(audio->Playing(voice));
}

TEST_CASE("鳴っている声は片付けても残る") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("keep.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    audio->Collect();
    CHECK(audio->VoiceCount() == 1);
    CHECK(audio->Playing(voice));
}

TEST_CASE("全て止められる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto first = audio->Load(MakeWave("all1.wav", 0.2));
    const auto second = audio->Load(MakeWave("all2.wav", 0.2));
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    audio->Play(*first);
    audio->Play(*second);
    audio->Play(*second);
    CHECK(audio->VoiceCount() == 3);

    audio->StopAll();
    CHECK(audio->VoiceCount() == 0);
}

TEST_CASE("上限を超えると優先度の低い声が消える") {
    auto audio = Platform::AudioDevice::Create(Silent(2));
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("limit.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId low = audio->Play(*sound, Base::PlaySettings{1.0, 1.0, false, 10});
    const Base::VoiceId high = audio->Play(*sound, Base::PlaySettings{1.0, 1.0, false, 50});
    CHECK(audio->VoiceCount() == 2);

    const Base::VoiceId added = audio->Play(*sound, Base::PlaySettings{1.0, 1.0, false, 30});
    CHECK(added != Base::VoiceId::None);
    CHECK(audio->VoiceCount() == 2);
    CHECK_FALSE(audio->Playing(low));
    CHECK(audio->Playing(high));
}

TEST_CASE("上限のとき優先度が足りなければ鳴らない") {
    auto audio = Platform::AudioDevice::Create(Silent(1));
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("deny.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId first = audio->Play(*sound, Base::PlaySettings{1.0, 1.0, false, 100});
    const Base::VoiceId second = audio->Play(*sound, Base::PlaySettings{1.0, 1.0, false, 20});
    CHECK(second == Base::VoiceId::None);
    CHECK(audio->VoiceCount() == 1);
    CHECK(audio->Playing(first));
}

TEST_CASE("音を指した指定はこれから鳴るものにも効く") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("apply.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId first = audio->Play(*sound);
    audio->SetGain(*sound, 0.25);
    audio->SetPitch(*sound, 2.0);

    const Base::VoiceId second = audio->Play(*sound);
    CHECK(first != Base::VoiceId::None);
    CHECK(second != Base::VoiceId::None);
    CHECK(audio->VoiceCount() == 2);

    audio->SetGain(*sound, 1.0, 0.5);
    audio->SetGain(Base::SoundId::None, 1.0);
    audio->SetPitch(Base::SoundId::None, 1.0);
    CHECK(audio->VoiceCount() == 2);
}

TEST_CASE("位置を移せる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("seek.wav", 1.0));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    CHECK(audio->Position(voice) == doctest::Approx(0.0));

    audio->Seek(voice, 0.5);
    CHECK(audio->Position(voice) == doctest::Approx(0.5).epsilon(0.01));

    audio->Seek(voice, -1.0);
    CHECK(audio->Position(voice) == doctest::Approx(0.0));
}

TEST_CASE("全体の音量を変えられる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    CHECK(audio->MasterGain() == doctest::Approx(1.0));

    audio->SetMasterGain(0.5);
    CHECK(audio->MasterGain() == doctest::Approx(0.5));
}

TEST_CASE("捨てた音は鳴らせない") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("free.wav", 0.2));
    REQUIRE(sound.has_value());

    audio->Play(*sound);
    audio->Free(*sound);
    CHECK(audio->SoundCount() == 0);
    CHECK(audio->VoiceCount() == 0);
    CHECK(audio->Play(*sound) == Base::VoiceId::None);
    CHECK(audio->Play(Base::SoundId::None) == Base::VoiceId::None);
}

TEST_CASE("装置は移せる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("move.wav", 0.2));
    REQUIRE(sound.has_value());
    audio->Play(*sound);

    Platform::AudioDevice moved = std::move(*audio);
    CHECK(moved.VoiceCount() == 1);
    CHECK(moved.SoundCount() == 1);
}

TEST_CASE("再生レートは読み書きできる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("rate.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    CHECK(audio->Pitch(voice) == doctest::Approx(1.0));
    CHECK(audio->Gain(voice) == doctest::Approx(1.0));

    audio->SetPitch(voice, 0.5);
    CHECK(audio->Pitch(voice) == doctest::Approx(0.5));

    audio->SetGain(voice, 0.25);
    CHECK(audio->Gain(voice) == doctest::Approx(0.25));

    CHECK(audio->Pitch(Base::VoiceId::None) == doctest::Approx(0.0));
    CHECK(audio->Gain(Base::VoiceId::None) == doctest::Approx(0.0));
}

TEST_CASE("再生レートは移植元の幅に収まる") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("clamp.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);

    audio->SetPitch(voice, 0.0);
    CHECK(audio->Pitch(voice) == doctest::Approx(Base::kMinPitch));

    audio->SetPitch(voice, -3.0);
    CHECK(audio->Pitch(voice) == doctest::Approx(Base::kMinPitch));

    audio->SetPitch(voice, 1000.0);
    CHECK(audio->Pitch(voice) == doctest::Approx(Base::kMaxPitch));

    const Base::VoiceId made = audio->Play(*sound, Base::PlaySettings{1.0, 0.0, false, 0});
    CHECK(audio->Pitch(made) == doctest::Approx(Base::kMinPitch));
}

TEST_CASE("再生レートを上げると早く鳴り終わる") {
    const auto path = MakeWave("speed.wav", 0.4);

    const auto remaining = [&path](double pitch) {
        auto audio = Platform::AudioDevice::Create(Silent());
        REQUIRE(audio.has_value());
        const auto sound = audio->Load(path);
        REQUIRE(sound.has_value());

        audio->Play(*sound, Base::PlaySettings{1.0, pitch, false, 0});

        // 元の長さの半分ぶんだけ進める
        audio->Mix(static_cast<std::uint64_t>(audio->SampleRate() * 0.21));
        audio->Collect();
        return audio->VoiceCount();
    };

    CHECK(remaining(1.0) == 1);
    CHECK(remaining(2.0) == 0);
}

TEST_CASE("鳴り終わった声は片付けで消える") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("finish.wav", 0.1));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    audio->Mix(static_cast<std::uint64_t>(audio->SampleRate() * 0.05));
    audio->Collect();
    CHECK(audio->VoiceCount() == 1);
    CHECK(audio->Playing(voice));

    audio->Mix(static_cast<std::uint64_t>(audio->SampleRate() * 0.1));
    CHECK_FALSE(audio->Playing(voice));

    audio->Collect();
    CHECK(audio->VoiceCount() == 0);
}

TEST_CASE("繰り返す声は終わらない") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("loop.wav", 0.1));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound, Base::PlaySettings{1.0, 1.0, true, 0});
    audio->Mix(static_cast<std::uint64_t>(audio->SampleRate() * 0.5));
    audio->Collect();
    CHECK(audio->VoiceCount() == 1);
    CHECK(audio->Playing(voice));
}

TEST_CASE("作りを選んでも同じように鳴る") {
    Platform::AudioSettings settings = Silent();
    settings.quality = Base::AudioQuality::High;

    auto audio = Platform::AudioDevice::Create(settings);
    REQUIRE(audio.has_value());
    const auto sound = audio->Load(MakeWave("quality.wav", 0.2));
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound, Base::PlaySettings{1.0, 1.5, false, 0});
    CHECK(audio->Pitch(voice) == doctest::Approx(1.5));
    CHECK(audio->Mix(1024) == 1024);
}

TEST_CASE("装置を開いていれば自分では進めない") {
    auto audio = Platform::AudioDevice::Create();
    if (!audio.has_value()) {
        MESSAGE("音声装置が無いので飛ばす");
        return;
    }
    CHECK(audio->Mix(1024) == 0);
    CHECK(audio->SampleRate() > 0);
}

TEST_CASE("中身から直に読める") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());

    const auto contents = Base::Files::ReadBytes(MakeWave("memory.wav", 0.3));
    REQUIRE(contents.has_value());

    const auto sound = audio->Load(TellerEngine::Span<const std::byte>{*contents});
    REQUIRE(sound.has_value());
    CHECK(audio->Length(*sound) == doctest::Approx(0.3).epsilon(0.01));

    const Base::VoiceId voice = audio->Play(*sound);
    CHECK(voice != Base::VoiceId::None);
    CHECK(audio->Playing(voice));

    audio->Free(*sound);
    CHECK(audio->SoundCount() == 0);
}

TEST_CASE("空の中身は失敗する") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());
    CHECK_FALSE(audio->Load(TellerEngine::Span<const std::byte>{}).has_value());

    const std::vector<std::byte> junk(64, std::byte{0x7F});
    CHECK_FALSE(audio->Load(TellerEngine::Span<const std::byte>{junk}).has_value());
    CHECK(audio->SoundCount() == 0);
}

TEST_CASE("同じ中身を何度も読める") {
    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());

    const auto contents = Base::Files::ReadBytes(MakeWave("twice.wav", 0.2));
    REQUIRE(contents.has_value());

    const auto first = audio->Load(TellerEngine::Span<const std::byte>{*contents});
    const auto second = audio->Load(TellerEngine::Span<const std::byte>{*contents});
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK(*first != *second);
    CHECK(audio->SoundCount() == 2);

    audio->Play(*first);
    audio->Play(*second);
    CHECK(audio->VoiceCount() == 2);
}

TEST_CASE("実際の装置でも鳴らせる") {
    auto audio = Platform::AudioDevice::Create();
    if (!audio.has_value()) {
        MESSAGE("音声装置が無いので飛ばす");
        return;
    }
    audio->SetMasterGain(0.0);

    const auto sound = audio->Load(MakeWave("device.wav", 0.2));
    REQUIRE(sound.has_value());
    CHECK(audio->Length(*sound) == doctest::Approx(0.2).epsilon(0.01));

    const Base::VoiceId voice = audio->Play(*sound);
    CHECK(voice != Base::VoiceId::None);
    CHECK(audio->VoiceCount() == 1);

    audio->StopAll();
    CHECK(audio->VoiceCount() == 0);
}

TEST_CASE("本家の音を読める") {
    const auto assets = AssetRoot();
    if (assets.empty() || !std::filesystem::is_directory(assets / "Sounds")) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());

    const auto sound = audio->Load(assets / "Sounds" / "SND_TXT1.wav");
    REQUIRE(sound.has_value());
    CHECK(audio->Length(*sound) > 0.0);

    const Base::VoiceId voice = audio->Play(*sound, Base::PlaySettings{0.8, 1.0, false, 80});
    CHECK(voice != Base::VoiceId::None);
    CHECK(audio->Playing(voice));
}

TEST_CASE("本家のoggを読める") {
    const auto assets = AssetRoot();
    if (assets.empty() || !std::filesystem::is_directory(assets / "Sounds")) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());

    const auto sound = audio->Load(assets / "Sounds" / "mus_a2.ogg");
    REQUIRE(sound.has_value());
    CHECK(audio->Length(*sound) > 1.0);

    const Base::VoiceId voice = audio->Play(*sound, Base::PlaySettings{1.0, 1.0, true, 120});
    CHECK(voice != Base::VoiceId::None);
    CHECK(audio->Playing(voice));

    audio->Seek(voice, 3.0);
    CHECK(audio->Position(voice) == doctest::Approx(3.0).epsilon(0.05));
}

TEST_CASE("本家のoggを中身から読める") {
    const auto assets = AssetRoot();
    if (assets.empty() || !std::filesystem::is_directory(assets / "Sounds")) {
        MESSAGE("TELLER_ASSETS が無いので飛ばす");
        return;
    }

    auto audio = Platform::AudioDevice::Create(Silent());
    REQUIRE(audio.has_value());

    const auto contents = Base::Files::ReadBytes(assets / "Sounds" / "abc_123_a.ogg");
    REQUIRE(contents.has_value());

    const auto sound = audio->Load(TellerEngine::Span<const std::byte>{*contents});
    REQUIRE(sound.has_value());

    const Base::VoiceId voice = audio->Play(*sound);
    CHECK(voice != Base::VoiceId::None);
    CHECK(audio->Playing(voice));
}
