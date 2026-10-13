#pragma once
#include <memory>
#include <string>

#include "audio/music_backend.h"

namespace sc2k {

// FluidSynth renderer (LGPL library, linked dynamically). Returns nullptr and sets `error` when the SoundFont cannot be loaded.
std::unique_ptr<MusicBackend> makeFluidBackend(const std::string& soundfont, int volume, int sampleRate, std::string* error);

}  // namespace sc2k
