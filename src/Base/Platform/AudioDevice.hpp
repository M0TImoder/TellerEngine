#pragma once

#include <Base/Audio.hpp>
#include <Base/Compat.hpp>
#include <Base/Error.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace TellerEngine::Base::Platform {

struct AudioSettings {
    // 同時に鳴らせる数
    std::size_t voiceLimit = 128;

    // 装置を開かずに動かす
    bool silent = false;

    // 開いた後は変えられない
    AudioQuality quality = AudioQuality::Original;
};

// miniaudioで鳴らす
class AudioDevice : public Audio {
public:
    static Expected<AudioDevice, Error> Create(const AudioSettings &settings = {});

    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;
    AudioDevice(AudioDevice &&other) noexcept;
    AudioDevice &operator=(AudioDevice &&other) noexcept;
    ~AudioDevice() override;

    Expected<SoundId, Error> Load(const std::filesystem::path &path);

    // 埋め込みの音を中身から読む
    Expected<SoundId, Error> Load(Span<const std::byte> contents);

    // 読み込んだ音を捨てる
    void Free(SoundId sound);

    std::size_t SoundCount() const;

    VoiceId Play(SoundId sound, const PlaySettings &settings = {}) override;

    void Stop(VoiceId voice) override;
    void Stop(SoundId sound) override;
    void StopAll() override;

    void Pause(VoiceId voice) override;
    void Resume(VoiceId voice) override;

    bool Playing(VoiceId voice) const override;
    bool Playing(SoundId sound) const override;

    void SetGain(VoiceId voice, double gain, double time = 0.0) override;
    void SetGain(SoundId sound, double gain, double time = 0.0) override;

    void SetPitch(VoiceId voice, double pitch) override;
    void SetPitch(SoundId sound, double pitch) override;

    double Gain(VoiceId voice) const override;
    double Pitch(VoiceId voice) const override;

    double Gain(SoundId sound) const override;
    double Pitch(SoundId sound) const override;

    double Position(VoiceId voice) const override;
    void Seek(VoiceId voice, double seconds) override;

    double Length(SoundId sound) const override;

    void SetMasterGain(double gain) override;
    double MasterGain() const override;

    void Collect() override;

    std::size_t VoiceCount() const override;

    // 装置を開いていないとき、指した数だけ進める
    std::uint64_t Mix(std::uint64_t frames);

    // 1秒あたりの標本数
    std::uint32_t SampleRate() const;

private:
    struct State;

    AudioDevice() = default;

    std::unique_ptr<State> state_;
};

} // namespace TellerEngine::Base::Platform
