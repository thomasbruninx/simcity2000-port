// A music renderer: plays a parsed MIDI file into stereo float frames. Two implementations: the port's own small synthesizer (MidiSynth, midi.h)
// and FluidSynth with a SoundFont (fluid_backend.h, optional at build time).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "audio/midi.h"

namespace sc2k {

// User choice of the music synthesizer (stored in the settings file).
struct AudioSettings {
    enum class Synth { Builtin, FluidSynth };
    Synth synth = Synth::Builtin;
    std::string soundfont;          // path of the .sf2 for FluidSynth ("" = first one found)
    int volume = 80;                // music volume 0..100 (FluidSynth gain scale)

    static std::string settingsPath();                  // ~/Library/Application Support/sc2k-port/settings.ini (or $XDG_CONFIG_HOME / ~/.config)
    static AudioSettings load();
    bool save() const;
    // .sf2 files in the usual places: ./resources, next to the executable (../resources, ../../resources), `extraDir`
    static std::vector<std::string> discoverSoundfonts(const std::string& extraDir = {});
};

bool fluidSynthAvailable();                              // built with FluidSynth support and the library loaded
// Creates the backend for the settings; falls back to the built-in synthesizer and fills `error` when FluidSynth cannot be used.
std::unique_ptr<MusicBackend> makeMusicBackend(const AudioSettings& s, int sampleRate, std::string* error = nullptr);

}  // namespace sc2k
