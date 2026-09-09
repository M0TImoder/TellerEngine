#pragma once

#include <cstdint>

namespace TellerEngine::Base {

// 読み込んだ音
enum class SoundId : std::uint32_t { None = 0 };

// 鳴っている1つ
enum class VoiceId : std::uint32_t { None = 0 };

// 再生レートを変えるときの作り
enum class AudioQuality {
    // 移植元と同じ線形補間
    Original,

    // 折り返しを抑える
    High,
};

// 移植元が受け付ける再生レートの幅
inline constexpr double kMinPitch = 1.0 / 256.0;
inline constexpr double kMaxPitch = 256.0;

struct PlaySettings {
    double gain = 1.0;

    // 再生レートの倍率
    double pitch = 1.0;

    bool loop = false;

    // 声が足りないときは小さい方から消える
    int priority = 0;
};

// 音を鳴らす口
class Audio {
public:
    virtual ~Audio() = default;

    virtual VoiceId Play(SoundId sound, const PlaySettings &settings = {}) = 0;

    virtual void Stop(VoiceId voice) = 0;

    // その音のものを全て止める
    virtual void Stop(SoundId sound) = 0;

    virtual void StopAll() = 0;

    virtual void Pause(VoiceId voice) = 0;
    virtual void Resume(VoiceId voice) = 0;

    virtual bool Playing(VoiceId voice) const = 0;
    virtual bool Playing(SoundId sound) const = 0;

    // timeが正なら、その秒数をかけて移る
    virtual void SetGain(VoiceId voice, double gain, double time = 0.0) = 0;

    // その音とこれから鳴るものに効く
    virtual void SetGain(SoundId sound, double gain, double time = 0.0) = 0;

    virtual void SetPitch(VoiceId voice, double pitch) = 0;
    virtual void SetPitch(SoundId sound, double pitch) = 0;

    virtual double Gain(VoiceId voice) const = 0;
    virtual double Pitch(VoiceId voice) const = 0;

    virtual double Gain(SoundId sound) const = 0;
    virtual double Pitch(SoundId sound) const = 0;

    // 頭からの秒数
    virtual double Position(VoiceId voice) const = 0;
    virtual void Seek(VoiceId voice, double seconds) = 0;

    virtual double Length(SoundId sound) const = 0;

    virtual void SetMasterGain(double gain) = 0;
    virtual double MasterGain() const = 0;

    // 鳴り終わった声を捨てる
    virtual void Collect() = 0;

    // 鳴っている声の数
    virtual std::size_t VoiceCount() const = 0;
};

} // namespace TellerEngine::Base
