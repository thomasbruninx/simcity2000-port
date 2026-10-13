// Sound effects and music. `SoundBank` reads SOUNDS/5xx.WAV (8-bit mono 11025 Hz PCM) and SOUNDS/100xx.MID of the player's installation;
// `Mixer` mixes the effects and the MIDI synth into interleaved stereo float frames (independent of SDL, so it can be tested);
// `AudioDevice` pulls from a Mixer through an SDL3 audio stream.
// Effect ids are the numbers the original passes to its sound routine 0x424AA0 (500 + n -> 5nn.WAV); music ids 10000..10018.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio/midi.h"
#include "audio/music_backend.h"

namespace sc2k {

struct Wave { std::vector<float> samples; int rate = 11025; };

class SoundBank {
public:
    explicit SoundBank(std::string dir) : dir_(std::move(dir)) {}
    const Wave* effect(int id);               // loads SOUNDS/<id>.WAV on first use; nullptr when missing
    const MidiFile* music(int id);            // SOUNDS/<id>.MID
    static Wave parseWave(const std::vector<uint8_t>& bytes);   // throws on unsupported formats
private:
    std::string dir_;
    std::map<int, std::unique_ptr<Wave>> waves_;
    std::map<int, std::unique_ptr<MidiFile>> midis_;
    std::map<int, bool> missing_;
};

class Mixer {
public:
    Mixer(SoundBank* bank, int sampleRate = 44100) : bank_(bank), rate_(sampleRate), synth_(std::make_unique<MidiSynth>(sampleRate)) {}
    // Replaces the music synthesizer (built-in or FluidSynth); the running tune restarts on the new one.
    void setBackend(std::unique_ptr<MusicBackend> backend);
    std::string backendName() const;
    void playEffect(int id, int repeat = 0);  // repeat = extra plays; -1 loops until stopEffect
    void stopEffect(int id);                  // id < 0 stops everything (0x424B10)
    void playMusic(int id);                   // replaces the running tune
    void stopMusic();
    bool musicPlaying() const;
    int lastMusicId() const { return musicId_; }
    void setEffectsEnabled(bool on) { effectsOn_ = on; if (!on) stopEffect(-1); }   // Options > Sound Effects
    void setMusicEnabled(bool on) { musicOn_ = on; if (!on) stopMusic(); }           // Options > Music
    bool musicEnabled() const { return musicOn_; }
    void setMuted(bool m) { muted_ = m; }
    bool muted() const { return muted_; }
    void render(float* out, int frames);      // overwrites `out` (interleaved stereo)
    int activeEffects() const;
private:
    struct Voice { int id = 0; const Wave* wave = nullptr; double pos = 0; int repeat = 0; };
    SoundBank* bank_;
    int rate_;
    mutable std::mutex mutex_;
    std::vector<Voice> voices_;
    std::unique_ptr<MusicBackend> synth_;
    int musicId_ = 0;
    bool muted_ = false, effectsOn_ = true, musicOn_ = true;
};

// Plays a Mixer through the default SDL playback device.
class AudioDevice {
public:
    explicit AudioDevice(Mixer& mixer);
    ~AudioDevice();
    bool ok() const { return stream_ != nullptr; }
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    void* stream_ = nullptr;
};

}  // namespace sc2k
