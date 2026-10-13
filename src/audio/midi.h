// Standard MIDI File reader and a small software synthesizer (no external sound font): the music of SimCity 2000 is ten-track SMF format 1 data
// (SOUNDS/100xx.MID). The original plays it through the Windows MIDI mapper (FM / wavetable synth of the player's sound card); the port renders it
// with simple waveforms per General MIDI instrument family and noise-based drums, so the tunes are recognisable but do not sound like the card.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sc2k {

struct MidiEvent {
    uint32_t tick = 0;
    uint8_t status = 0, a = 0, b = 0;     // channel messages (status includes the channel); meta tempo is stored as status 0xFF, a = 0x51, value in `tempo`
    uint32_t tempo = 0;                   // microseconds per quarter note for tempo events
};

struct MidiFile {
    int division = 192;                   // ticks per quarter note
    std::vector<MidiEvent> events;        // all tracks merged, sorted by tick (stable)
    uint32_t lengthTicks = 0;
    double seconds() const;               // duration following the tempo map
    static MidiFile parse(const std::vector<uint8_t>& bytes);   // throws std::runtime_error on malformed data
    static MidiFile load(const std::string& path);
};

class MidiSynth {
public:
    explicit MidiSynth(int sampleRate = 44100) : rate_(sampleRate) {}
    void start(const MidiFile* file);                 // the file must outlive playback
    void stop();
    bool playing() const { return file_ != nullptr && !finished_; }
    // Adds `frames` stereo float frames to `out` (interleaved L R) and advances the sequencer.
    void render(float* out, int frames);
    int activeVoices() const;

private:
    struct Voice { bool on = false, released = false; int channel = 0, note = 0; float velocity = 0, phase = 0, env = 0, age = 0, freq = 0; int family = 0; float noise = 0; };
    void handle(const MidiEvent& e);
    void noteOn(int ch, int note, int vel);
    void noteOff(int ch, int note);
    int rate_;
    const MidiFile* file_ = nullptr;
    size_t next_ = 0;
    double tick_ = 0;
    uint32_t tempo_ = 500000;
    bool finished_ = true;
    std::array<uint8_t, 16> program_{}, volume_, expression_;
    std::array<float, 16> bend_{};
    std::array<Voice, 40> voices_{};
    uint32_t rng_ = 12345;
    float nextNoise();
    void init();
};

}  // namespace sc2k
