#include "test.h"
#include "audio/audio.h"
#include "audio/midi.h"

#include <cmath>
#include <cstdlib>
#include <string>

using namespace sc2k;

TEST(midi_parser_reads_a_minimal_file) {
    // header: format 0, 1 track, 96 ticks/quarter; track: tempo 120 bpm, note on C4 at 0, note off at 96, end of track at 96
    const std::vector<uint8_t> f = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96, 'M', 'T', 'r', 'k', 0, 0, 0, 20,
                                    0x00, 0xFF, 0x51, 3, 0x07, 0xA1, 0x20, 0x00, 0x90, 60, 100, 0x60, 0x80, 60, 0, 0x00, 0xFF, 0x2F, 0};
    const MidiFile m = MidiFile::parse(f);
    CHECK(m.division == 96 && m.events.size() == 3 && m.lengthTicks == 96);
    CHECK(m.events[1].status == 0x90 && m.events[1].a == 60 && m.events[2].tick == 96);
    CHECK(std::fabs(m.seconds() - 0.5) < 1e-9);
    MidiSynth synth(8000);
    synth.start(&m);
    std::vector<float> out(2 * 8000, 0.f);
    synth.render(out.data(), 8000);
    float peak = 0; for (float v : out) peak = std::max(peak, std::fabs(v));
    CHECK(peak > 0.01f && peak <= 1.0f);
}

TEST(wave_parser_converts_8_bit_pcm) {
    std::vector<uint8_t> w = {'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0, 0x11, 0x2B, 0, 0, 0x11, 0x2B, 0, 0, 1, 0, 8, 0,
                              'd', 'a', 't', 'a', 4, 0, 0, 0, 128, 255, 0, 128};
    const Wave a = SoundBank::parseWave(w);
    CHECK(a.rate == 11025 && a.samples.size() == 4 && std::fabs(a.samples[0]) < 1e-6f && a.samples[1] > 0.99f && a.samples[2] < -0.99f);
}

TEST(original_sounds_and_music_load_and_render) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    SoundBank bank(root);
    const Wave* boom = bank.effect(504);
    CHECK(boom && boom->rate == 11025 && boom->samples.size() > 10000);
    CHECK(bank.effect(9999) == nullptr);
    const MidiFile* tune = bank.music(10001);
    CHECK(tune && tune->division == 192 && !tune->events.empty() && tune->seconds() > 20.0);
    Mixer mixer(&bank);
    mixer.playMusic(10001);
    mixer.playEffect(504);
    std::vector<float> out(2 * 44100, 0.f);
    float peak = 0; int frames = 0;
    for (int i = 0; i < 6; ++i) { mixer.render(out.data(), 44100); for (float v : out) peak = std::max(peak, std::fabs(v)); frames += 44100; }
    CHECK(peak > 0.05f && mixer.musicPlaying());
    CHECK(mixer.activeEffects() == 0);   // the short effect has ended
    mixer.playEffect(504, -1);
    mixer.render(out.data(), 1000);
    CHECK(mixer.activeEffects() == 1);
    mixer.stopEffect(-1);
    CHECK(mixer.activeEffects() == 0);
    for (int id = 10000; id <= 10018; ++id) CHECK(bank.music(id) != nullptr);
}

TEST(fluidsynth_backend_renders_the_music_when_available) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root || !fluidSynthAvailable()) { std::printf("  (skipped: no game data or no FluidSynth)\n"); return; }
    const auto fonts = AudioSettings::discoverSoundfonts();
    if (fonts.empty()) { std::printf("  (skipped: no .sf2 found)\n"); return; }
    AudioSettings s; s.synth = AudioSettings::Synth::FluidSynth; s.soundfont = fonts.front();
    std::string err;
    auto backend = makeMusicBackend(s, 44100, &err);
    CHECK(backend && backend->name() == "FluidSynth" && err.empty());
    SoundBank bank(root);
    Mixer mixer(&bank);
    mixer.setBackend(std::move(backend));
    mixer.playMusic(10001);
    std::vector<float> out(2 * 44100, 0.f);
    float peak = 0;
    for (int i = 0; i < 6; ++i) { mixer.render(out.data(), 44100); for (float v : out) peak = std::max(peak, std::fabs(v)); }
    CHECK(peak > 0.02f && peak <= 1.0f && mixer.musicPlaying());
    s.soundfont = "/nonexistent.sf2";
    auto fallback = makeMusicBackend(s, 44100, &err);
    CHECK(fallback && fallback->name() == "Built-in synthesizer" && !err.empty());
}

TEST(effects_play_out_side_by_side) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    SoundBank bank(root);
    Mixer mixer(&bank);
    mixer.playEffect(515);          // the crash whine
    mixer.playEffect(504);          // the explosion that follows it
    mixer.playEffect(507, -1);      // the fire loop is (re)started every frame
    mixer.playEffect(507, -1);
    std::vector<float> out(2 * 200, 0.f);
    mixer.render(out.data(), 200);
    CHECK(mixer.activeEffects() == 3);   // none of them cut the others off; the loop runs once
    mixer.playEffect(504);
    CHECK(mixer.activeEffects() == 3);   // the same explosion again within a few milliseconds is not started twice
    for (int i = 0; i < 20; ++i) mixer.playEffect(500 + i % 5 + 10 * (i % 2), 0);
    CHECK(mixer.activeEffects() <= 8);
}
