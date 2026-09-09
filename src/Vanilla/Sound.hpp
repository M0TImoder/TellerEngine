#pragma once

// 移植元が音を鳴らすときに通る道

#include <Base/Audio.hpp>
#include <Vanilla/MusicIndex.hpp>

namespace TellerEngine::Vanilla {

// 効果音、曲、繰り返す曲の順に残りやすい
inline constexpr int kSoundPriority = 80;
inline constexpr int kMusicPriority = 100;
inline constexpr int kLoopPriority = 120;

// Original: snd_play
inline Base::VoiceId PlaySound(Base::Audio &audio, Base::SoundId sound) {
    return audio.Play(sound, Base::PlaySettings{1.0, 1.0, false, kSoundPriority});
}

// Original: snd_stop
inline void StopSound(Base::Audio &audio, Base::SoundId sound) { audio.Stop(sound); }

// Original: snd_isplaying
inline bool SoundPlaying(const Base::Audio &audio, Base::SoundId sound) {
    return audio.Playing(sound);
}

// Original: caster_play
// 音量と再生レートはアセットに効く
inline Base::VoiceId PlayMusic(Base::Audio &audio, Base::SoundId sound, double volume,
                               double pitch) {
    const Base::VoiceId voice =
        audio.Play(sound, Base::PlaySettings{1.0, 1.0, false, kMusicPriority});
    audio.SetPitch(sound, pitch);
    audio.SetGain(sound, volume);
    return voice;
}

// Original: caster_play_l
// 音量と再生レートは鳴らした1つだけに効く
inline Base::VoiceId PlayMusicLocal(Base::Audio &audio, Base::SoundId sound, double volume,
                                    double pitch) {
    const Base::VoiceId voice =
        audio.Play(sound, Base::PlaySettings{1.0, 1.0, false, kMusicPriority});
    audio.SetPitch(voice, pitch);
    audio.SetGain(voice, volume);
    return voice;
}

// Original: caster_loop
inline Base::VoiceId LoopMusic(Base::Audio &audio, Base::SoundId sound, double volume,
                               double pitch) {
    const Base::VoiceId voice =
        audio.Play(sound, Base::PlaySettings{1.0, 1.0, true, kLoopPriority});
    audio.SetPitch(sound, pitch);
    audio.SetGain(sound, volume);
    return voice;
}

// Original: caster_free
inline void FreeMusic(Base::Audio &audio, Base::SoundId sound) { audio.Stop(sound); }

// Original: caster_free(all)
inline void FreeAllMusic(Base::Audio &audio) { audio.StopAll(); }

// Original: caster_stop
inline void StopMusic(Base::Audio &audio, Base::SoundId sound) { FreeMusic(audio, sound); }

// Original: caster_pause
inline void PauseMusic(Base::Audio &audio, Base::VoiceId voice) { audio.Pause(voice); }

// Original: caster_resume
inline void ResumeMusic(Base::Audio &audio, Base::VoiceId voice) { audio.Resume(voice); }

// Original: caster_set_pitch
inline void SetMusicPitch(Base::Audio &audio, Base::SoundId sound, double pitch) {
    audio.SetPitch(sound, pitch);
}

// Original: caster_set_volume
inline void SetMusicVolume(Base::Audio &audio, Base::SoundId sound, double volume) {
    audio.SetGain(sound, volume);
}

// Original: caster_get_pitch
inline double MusicPitch(const Base::Audio &audio, Base::SoundId sound) {
    return audio.Pitch(sound);
}

// Original: caster_get_volume
inline double MusicVolume(const Base::Audio &audio, Base::SoundId sound) {
    return audio.Gain(sound);
}

// Original: caster_is_playing
inline bool MusicPlaying(const Base::Audio &audio, Base::SoundId sound) {
    return audio.Playing(sound);
}

// Original: caster_set_panning
inline void SetMusicPanning(Base::Audio &, Base::SoundId, double) {}

} // namespace TellerEngine::Vanilla
