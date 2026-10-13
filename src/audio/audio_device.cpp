#include <SDL3/SDL.h>

#include <vector>

#include "audio/audio.h"

namespace sc2k {

struct AudioDevice::Impl {
    Mixer* mixer;
    std::vector<float> buffer;
};

AudioDevice::AudioDevice(Mixer& mixer) : impl_(std::make_unique<Impl>()) {
    impl_->mixer = &mixer;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return;
    SDL_AudioSpec spec; spec.format = SDL_AUDIO_F32; spec.channels = 2; spec.freq = 44100;
    SDL_AudioStream* s = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
        [](void* user, SDL_AudioStream* stream, int additional, int) {
            Impl* im = static_cast<Impl*>(user);
            const int frames = additional / 8;
            if (frames <= 0) return;
            im->buffer.resize(static_cast<size_t>(frames) * 2);
            im->mixer->render(im->buffer.data(), frames);
            SDL_PutAudioStreamData(stream, im->buffer.data(), frames * 8);
        }, impl_.get());
    if (!s) return;
    stream_ = s;
    SDL_ResumeAudioStreamDevice(s);
}

AudioDevice::~AudioDevice() {
    if (stream_) SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(stream_));
}

}  // namespace sc2k
