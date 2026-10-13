// Renders the game's sounds with the port's mixer / synth to a WAV file so they can be listened to without starting the game.
//   sc2k-audio <SC2K dir> music <id 10000..10018> out.wav [seconds]
//   sc2k-audio <SC2K dir> effect <id 500..530> out.wav
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "audio/audio.h"

using namespace sc2k;

int main(int argc, char** argv) {
    if (argc < 5) { std::fprintf(stderr, "usage: sc2k-audio <SC2K dir> music|effect <id> out.wav [seconds]\n"); return 2; }
    SoundBank bank(argv[1]);
    Mixer mixer(&bank);
    const std::string kind = argv[2];
    const int id = std::atoi(argv[3]);
    double seconds = argc > 5 ? std::atof(argv[5]) : 0.0;
    if (kind == "music") {
        const MidiFile* m = bank.music(id);
        if (!m) { std::fprintf(stderr, "no such tune\n"); return 1; }
        if (seconds <= 0) seconds = m->seconds() + 1.0;
        mixer.playMusic(id);
    } else {
        const Wave* w = bank.effect(id);
        if (!w) { std::fprintf(stderr, "no such effect\n"); return 1; }
        if (seconds <= 0) seconds = static_cast<double>(w->samples.size()) / w->rate + 0.2;
        mixer.playEffect(id);
    }
    const int frames = static_cast<int>(seconds * 44100);
    std::vector<float> out(static_cast<size_t>(frames) * 2);
    for (int done = 0; done < frames; done += 4096) mixer.render(out.data() + static_cast<size_t>(done) * 2, std::min(4096, frames - done));
    std::vector<int16_t> pcm(out.size());
    for (size_t i = 0; i < out.size(); ++i) pcm[i] = static_cast<int16_t>(std::max(-1.f, std::min(1.f, out[i])) * 32767.f);
    FILE* f = std::fopen(argv[4], "wb");
    if (!f) return 1;
    const uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * 2), rate = 44100, byteRate = rate * 4;
    const uint16_t fmt = 1, ch = 2, align = 4, bits = 16;
    const uint32_t riff = 36 + dataBytes, fmtSize = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f); std::fwrite(&fmtSize, 4, 1, f);
    std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f); std::fwrite(&byteRate, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dataBytes, 4, 1, f); std::fwrite(pcm.data(), 2, pcm.size(), f);
    std::fclose(f);
    std::printf("wrote %s (%.1f s)\n", argv[4], seconds);
    return 0;
}
