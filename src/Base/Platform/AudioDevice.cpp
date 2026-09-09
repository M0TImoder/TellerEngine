#include <Base/Platform/AudioDevice.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

// 宣言だけを先に出すとminiaudioのvorbisの口が開く
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#pragma GCC diagnostic pop

namespace TellerEngine::Base::Platform {

namespace {

constexpr ma_uint32 kSoundFlags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;

constexpr ma_uint32 kSilentSampleRate = 44100;
constexpr ma_uint32 kSilentChannels = 2;

// 一度に混ぜる長さ
constexpr std::uint64_t kMixChunk = 4096;

double ClampPitch(double pitch) {
    return std::clamp(pitch, kMinPitch, kMaxPitch);
}

} // namespace

struct AudioDevice::State {
    ma_engine engine{};
    bool ready = false;
    AudioSettings settings;

    struct Loaded {
        std::unique_ptr<ma_sound> prototype;

        // 埋め込みのときは中身をここで抱える
        std::vector<std::byte> held;
        std::string registered;

        double gain = 1.0;
        double pitch = 1.0;
    };

    struct Voice {
        std::unique_ptr<ma_sound> sound;
        std::uint32_t id = 0;
        SoundId source = SoundId::None;
        int priority = 0;
        bool paused = false;
    };

    std::vector<Loaded> sounds;
    std::vector<Voice> voices;
    std::uint32_t nextVoice = 1;
    std::uint32_t nextName = 1;

    ~State() {
        voices.clear();
        for (Loaded &loaded : sounds) {
            if (loaded.prototype != nullptr) {
                ma_sound_uninit(loaded.prototype.get());
            }
            if (!loaded.registered.empty()) {
                ma_resource_manager_unregister_data(ma_engine_get_resource_manager(&engine),
                                                    loaded.registered.c_str());
            }
        }
        sounds.clear();
        if (ready) {
            ma_engine_uninit(&engine);
        }
    }

    Loaded *Find(SoundId sound) {
        const auto index = static_cast<std::uint32_t>(sound);
        if (index < 1 || index > sounds.size()) {
            return nullptr;
        }
        Loaded &loaded = sounds[index - 1];
        return loaded.prototype == nullptr ? nullptr : &loaded;
    }

    const Loaded *Find(SoundId sound) const {
        return const_cast<State *>(this)->Find(sound);
    }

    Voice *Find(VoiceId voice) {
        const auto id = static_cast<std::uint32_t>(voice);
        for (Voice &entry : voices) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    const Voice *Find(VoiceId voice) const {
        return const_cast<State *>(this)->Find(voice);
    }

    void Drop(std::size_t index) {
        ma_sound_uninit(voices[index].sound.get());
        voices.erase(voices.begin() + static_cast<std::ptrdiff_t>(index));
    }
};

Expected<AudioDevice, Error> AudioDevice::Create(const AudioSettings &settings) {
    AudioDevice device;
    device.state_ = std::make_unique<State>();
    device.state_->settings = settings;

    ma_engine_config config = ma_engine_config_init();
    config.pitchResampling =
        ma_resampler_config_init(ma_format_f32, 0, 0, 0, ma_resample_algorithm_linear);
    config.pitchResampling.linear.lpfOrder =
        settings.quality == AudioQuality::High ? MA_MAX_FILTER_ORDER : 0;

    if (settings.silent) {
        config.noDevice = MA_TRUE;
        config.channels = kSilentChannels;
        config.sampleRate = kSilentSampleRate;
    }

    if (ma_engine_init(&config, &device.state_->engine) != MA_SUCCESS) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "音声装置を開けない"});
    }
    device.state_->ready = true;
    return device;
}

AudioDevice::AudioDevice(AudioDevice &&other) noexcept : state_(std::move(other.state_)) {}

AudioDevice &AudioDevice::operator=(AudioDevice &&other) noexcept {
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

AudioDevice::~AudioDevice() = default;

Expected<SoundId, Error> AudioDevice::Load(const std::filesystem::path &path) {
    if (state_ == nullptr) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "音声装置が無い"});
    }

    auto prototype = std::make_unique<ma_sound>();
    const ma_result result = ma_sound_init_from_file(
        &state_->engine, path.string().c_str(), kSoundFlags, nullptr, nullptr, prototype.get());
    if (result != MA_SUCCESS) {
        return Unexpected<Error>(Error{ErrorCode::ReadFailed, path.string()});
    }

    State::Loaded loaded;
    loaded.prototype = std::move(prototype);
    state_->sounds.push_back(std::move(loaded));
    return static_cast<SoundId>(state_->sounds.size());
}

void AudioDevice::Free(SoundId sound) {
    if (state_ == nullptr) {
        return;
    }
    Stop(sound);
    State::Loaded *loaded = state_->Find(sound);
    if (loaded == nullptr) {
        return;
    }
    ma_sound_uninit(loaded->prototype.get());
    loaded->prototype.reset();
    if (!loaded->registered.empty()) {
        ma_resource_manager_unregister_data(ma_engine_get_resource_manager(&state_->engine),
                                            loaded->registered.c_str());
        loaded->registered.clear();
    }
    loaded->held.clear();
    loaded->held.shrink_to_fit();
}

Expected<SoundId, Error> AudioDevice::Load(Span<const std::byte> contents) {
    if (state_ == nullptr) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "音声装置が無い"});
    }
    if (contents.empty()) {
        return Unexpected<Error>(Error{ErrorCode::Malformed, "音が空"});
    }

    State::Loaded loaded;
    loaded.held.assign(contents.begin(), contents.end());
    loaded.registered = "teller:" + std::to_string(state_->nextName);
    state_->nextName += 1;

    ma_resource_manager *manager = ma_engine_get_resource_manager(&state_->engine);
    if (ma_resource_manager_register_encoded_data(manager, loaded.registered.c_str(),
                                                  loaded.held.data(),
                                                  loaded.held.size()) != MA_SUCCESS) {
        return Unexpected<Error>(Error{ErrorCode::Unavailable, "音を登録できない"});
    }

    auto prototype = std::make_unique<ma_sound>();
    if (ma_sound_init_from_file(&state_->engine, loaded.registered.c_str(), kSoundFlags,
                                nullptr, nullptr, prototype.get()) != MA_SUCCESS) {
        ma_resource_manager_unregister_data(manager, loaded.registered.c_str());
        return Unexpected<Error>(Error{ErrorCode::Malformed, "音を読めない"});
    }

    loaded.prototype = std::move(prototype);
    state_->sounds.push_back(std::move(loaded));
    return static_cast<SoundId>(state_->sounds.size());
}

std::size_t AudioDevice::SoundCount() const {
    if (state_ == nullptr) {
        return 0;
    }
    std::size_t count = 0;
    for (const State::Loaded &loaded : state_->sounds) {
        if (loaded.prototype != nullptr) {
            count += 1;
        }
    }
    return count;
}

VoiceId AudioDevice::Play(SoundId sound, const PlaySettings &settings) {
    if (state_ == nullptr) {
        return VoiceId::None;
    }
    State::Loaded *loaded = state_->Find(sound);
    if (loaded == nullptr) {
        return VoiceId::None;
    }

    if (state_->voices.size() >= state_->settings.voiceLimit) {
        std::size_t lowest = 0;
        for (std::size_t i = 1; i < state_->voices.size(); ++i) {
            if (state_->voices[i].priority < state_->voices[lowest].priority) {
                lowest = i;
            }
        }
        if (state_->voices[lowest].priority > settings.priority) {
            return VoiceId::None;
        }
        state_->Drop(lowest);
    }

    auto copy = std::make_unique<ma_sound>();
    if (ma_sound_init_copy(&state_->engine, loaded->prototype.get(), kSoundFlags, nullptr,
                           copy.get()) != MA_SUCCESS) {
        return VoiceId::None;
    }

    ma_sound_set_volume(copy.get(), static_cast<float>(settings.gain * loaded->gain));
    ma_sound_set_pitch(copy.get(),
                       static_cast<float>(ClampPitch(settings.pitch * loaded->pitch)));
    ma_sound_set_looping(copy.get(), settings.loop ? MA_TRUE : MA_FALSE);
    if (ma_sound_start(copy.get()) != MA_SUCCESS) {
        ma_sound_uninit(copy.get());
        return VoiceId::None;
    }

    State::Voice voice;
    voice.sound = std::move(copy);
    voice.id = state_->nextVoice;
    voice.source = sound;
    voice.priority = settings.priority;
    state_->nextVoice += 1;
    state_->voices.push_back(std::move(voice));
    return static_cast<VoiceId>(state_->voices.back().id);
}

void AudioDevice::Stop(VoiceId voice) {
    if (state_ == nullptr) {
        return;
    }
    const auto id = static_cast<std::uint32_t>(voice);
    for (std::size_t i = 0; i < state_->voices.size(); ++i) {
        if (state_->voices[i].id == id) {
            state_->Drop(i);
            return;
        }
    }
}

void AudioDevice::Stop(SoundId sound) {
    if (state_ == nullptr) {
        return;
    }
    for (std::size_t i = state_->voices.size(); i > 0; --i) {
        if (state_->voices[i - 1].source == sound) {
            state_->Drop(i - 1);
        }
    }
}

void AudioDevice::StopAll() {
    if (state_ == nullptr) {
        return;
    }
    for (std::size_t i = state_->voices.size(); i > 0; --i) {
        state_->Drop(i - 1);
    }
}

void AudioDevice::Pause(VoiceId voice) {
    if (state_ == nullptr) {
        return;
    }
    if (State::Voice *found = state_->Find(voice)) {
        ma_sound_stop(found->sound.get());
        found->paused = true;
    }
}

void AudioDevice::Resume(VoiceId voice) {
    if (state_ == nullptr) {
        return;
    }
    if (State::Voice *found = state_->Find(voice)) {
        ma_sound_start(found->sound.get());
        found->paused = false;
    }
}

bool AudioDevice::Playing(VoiceId voice) const {
    if (state_ == nullptr) {
        return false;
    }
    const State::Voice *found = state_->Find(voice);
    return found != nullptr && ma_sound_is_playing(found->sound.get()) == MA_TRUE;
}

bool AudioDevice::Playing(SoundId sound) const {
    if (state_ == nullptr) {
        return false;
    }
    for (const State::Voice &voice : state_->voices) {
        if (voice.source == sound && ma_sound_is_playing(voice.sound.get()) == MA_TRUE) {
            return true;
        }
    }
    return false;
}

void AudioDevice::SetGain(VoiceId voice, double gain, double time) {
    if (state_ == nullptr) {
        return;
    }
    State::Voice *found = state_->Find(voice);
    if (found == nullptr) {
        return;
    }
    if (time > 0.0) {
        ma_sound_set_fade_in_milliseconds(found->sound.get(), -1.0f,
                                          static_cast<float>(gain),
                                          static_cast<ma_uint64>(time * 1000.0));
        return;
    }
    ma_sound_set_volume(found->sound.get(), static_cast<float>(gain));
}

void AudioDevice::SetGain(SoundId sound, double gain, double time) {
    if (state_ == nullptr) {
        return;
    }
    State::Loaded *loaded = state_->Find(sound);
    if (loaded == nullptr) {
        return;
    }
    loaded->gain = gain;
    for (State::Voice &voice : state_->voices) {
        if (voice.source == sound) {
            SetGain(static_cast<VoiceId>(voice.id), gain, time);
        }
    }
}

void AudioDevice::SetPitch(VoiceId voice, double pitch) {
    if (state_ == nullptr) {
        return;
    }
    if (State::Voice *found = state_->Find(voice)) {
        ma_sound_set_pitch(found->sound.get(), static_cast<float>(ClampPitch(pitch)));
    }
}

void AudioDevice::SetPitch(SoundId sound, double pitch) {
    if (state_ == nullptr) {
        return;
    }
    State::Loaded *loaded = state_->Find(sound);
    if (loaded == nullptr) {
        return;
    }
    loaded->pitch = ClampPitch(pitch);
    for (State::Voice &voice : state_->voices) {
        if (voice.source == sound) {
            ma_sound_set_pitch(voice.sound.get(), static_cast<float>(loaded->pitch));
        }
    }
}

double AudioDevice::Gain(VoiceId voice) const {
    if (state_ == nullptr) {
        return 0.0;
    }
    const State::Voice *found = state_->Find(voice);
    return found == nullptr ? 0.0 : ma_sound_get_volume(found->sound.get());
}

double AudioDevice::Pitch(VoiceId voice) const {
    if (state_ == nullptr) {
        return 0.0;
    }
    const State::Voice *found = state_->Find(voice);
    return found == nullptr ? 0.0 : ma_sound_get_pitch(found->sound.get());
}

double AudioDevice::Gain(SoundId sound) const {
    if (state_ == nullptr) {
        return 0.0;
    }
    const State::Loaded *loaded = state_->Find(sound);
    return loaded == nullptr ? 0.0 : loaded->gain;
}

double AudioDevice::Pitch(SoundId sound) const {
    if (state_ == nullptr) {
        return 0.0;
    }
    const State::Loaded *loaded = state_->Find(sound);
    return loaded == nullptr ? 0.0 : loaded->pitch;
}

double AudioDevice::Position(VoiceId voice) const {
    if (state_ == nullptr) {
        return 0.0;
    }
    const State::Voice *found = state_->Find(voice);
    if (found == nullptr) {
        return 0.0;
    }
    float cursor = 0.0f;
    ma_sound_get_cursor_in_seconds(found->sound.get(), &cursor);
    return cursor;
}

void AudioDevice::Seek(VoiceId voice, double seconds) {
    if (state_ == nullptr) {
        return;
    }
    State::Voice *found = state_->Find(voice);
    if (found == nullptr) {
        return;
    }
    const double rate = SampleRate();
    const double frame = std::max(0.0, seconds) * rate;
    ma_sound_seek_to_pcm_frame(found->sound.get(), static_cast<ma_uint64>(frame));
}

double AudioDevice::Length(SoundId sound) const {
    if (state_ == nullptr) {
        return 0.0;
    }
    const State::Loaded *loaded = state_->Find(sound);
    if (loaded == nullptr) {
        return 0.0;
    }
    float length = 0.0f;
    ma_sound_get_length_in_seconds(loaded->prototype.get(), &length);
    return length;
}

void AudioDevice::SetMasterGain(double gain) {
    if (state_ != nullptr) {
        ma_engine_set_volume(&state_->engine, static_cast<float>(gain));
    }
}

double AudioDevice::MasterGain() const {
    return state_ == nullptr ? 0.0 : ma_engine_get_volume(&state_->engine);
}

void AudioDevice::Collect() {
    if (state_ == nullptr) {
        return;
    }
    for (std::size_t i = state_->voices.size(); i > 0; --i) {
        State::Voice &voice = state_->voices[i - 1];
        if (!voice.paused && ma_sound_is_playing(voice.sound.get()) != MA_TRUE) {
            state_->Drop(i - 1);
        }
    }
}

std::size_t AudioDevice::VoiceCount() const {
    return state_ == nullptr ? 0 : state_->voices.size();
}

std::uint64_t AudioDevice::Mix(std::uint64_t frames) {
    if (state_ == nullptr || !state_->settings.silent) {
        return 0;
    }

    const auto channels = ma_engine_get_channels(&state_->engine);
    std::vector<float> scratch(static_cast<std::size_t>(kMixChunk * channels), 0.0f);

    std::uint64_t total = 0;
    while (total < frames) {
        const std::uint64_t want = std::min(kMixChunk, frames - total);
        ma_uint64 read = 0;
        if (ma_engine_read_pcm_frames(&state_->engine, scratch.data(), want, &read) !=
            MA_SUCCESS) {
            break;
        }
        total += read;
        if (read < want) {
            break;
        }
    }
    return total;
}

std::uint32_t AudioDevice::SampleRate() const {
    return state_ == nullptr ? 0 : ma_engine_get_sample_rate(&state_->engine);
}

} // namespace TellerEngine::Base::Platform
