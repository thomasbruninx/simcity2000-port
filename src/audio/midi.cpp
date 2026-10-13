#include "audio/midi.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace sc2k {

namespace {
uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint16_t be16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }
}  // namespace

MidiFile MidiFile::parse(const std::vector<uint8_t>& d) {
    if (d.size() < 14 || std::string(d.begin(), d.begin() + 4) != "MThd") throw std::runtime_error("not a MIDI file");
    MidiFile f;
    const int tracks = be16(&d[10]);
    f.division = be16(&d[12]);
    if (f.division & 0x8000) throw std::runtime_error("SMPTE time division is not supported");
    size_t pos = 8 + be32(&d[4]);
    std::vector<MidiEvent> all;
    for (int t = 0; t < tracks && pos + 8 <= d.size(); ++t) {
        if (std::string(d.begin() + static_cast<long>(pos), d.begin() + static_cast<long>(pos) + 4) != "MTrk") throw std::runtime_error("track chunk expected");
        const size_t len = be32(&d[pos + 4]), end = std::min(d.size(), pos + 8 + len);
        pos += 8;
        uint32_t tick = 0;
        uint8_t running = 0;
        auto vlq = [&] { uint32_t v = 0; while (pos < end) { const uint8_t b = d[pos++]; v = (v << 7) | (b & 0x7F); if (!(b & 0x80)) break; } return v; };
        while (pos < end) {
            tick += vlq();
            if (pos >= end) break;
            uint8_t status = d[pos];
            if (status & 0x80) { ++pos; if (status < 0xF0) running = status; } else status = running;
            if (status == 0xFF) {
                const uint8_t type = d[pos++];
                const uint32_t n = vlq();
                if (type == 0x51 && n == 3 && pos + 3 <= end) {
                    MidiEvent e; e.tick = tick; e.status = 0xFF; e.a = 0x51; e.tempo = uint32_t(d[pos]) << 16 | uint32_t(d[pos + 1]) << 8 | d[pos + 2];
                    all.push_back(e);
                }
                pos += n;
                if (type == 0x2F) break;
            } else if (status == 0xF0 || status == 0xF7) {
                pos += vlq();
            } else {
                const int args = (status >> 4) == 0xC || (status >> 4) == 0xD ? 1 : 2;
                if (pos + static_cast<size_t>(args) > end) break;
                MidiEvent e; e.tick = tick; e.status = status; e.a = d[pos]; e.b = args == 2 ? d[pos + 1] : 0;
                pos += static_cast<size_t>(args);
                all.push_back(e);
            }
        }
        f.lengthTicks = std::max(f.lengthTicks, tick);
        pos = end;
    }
    std::stable_sort(all.begin(), all.end(), [](const MidiEvent& x, const MidiEvent& y) { return x.tick < y.tick; });
    f.events = std::move(all);
    return f;
}

MidiFile MidiFile::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    return parse(std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {}));
}

double MidiFile::seconds() const {
    double t = 0; uint32_t last = 0, tempo = 500000;
    for (const MidiEvent& e : events) {
        if (e.status != 0xFF) continue;
        t += double(e.tick - last) * tempo / (1e6 * division);
        last = e.tick; tempo = e.tempo;
    }
    return t + double(lengthTicks - last) * tempo / (1e6 * division);
}

void MidiSynth::init() {
    program_.fill(0); volume_.fill(100); expression_.fill(127); bend_.fill(0);
    for (Voice& v : voices_) v = Voice{};
    next_ = 0; tick_ = 0; tempo_ = 500000;
}

void MidiSynth::start(const MidiFile* file) { file_ = file; init(); finished_ = file == nullptr; }
void MidiSynth::stop() { file_ = nullptr; finished_ = true; }
int MidiSynth::activeVoices() const { int n = 0; for (const Voice& v : voices_) n += v.on; return n; }
float MidiSynth::nextNoise() { rng_ = rng_ * 1664525u + 1013904223u; return static_cast<float>(static_cast<int32_t>(rng_)) / 2147483648.f; }

void MidiSynth::noteOn(int ch, int note, int vel) {
    if (vel == 0) { noteOff(ch, note); return; }
    Voice* slot = nullptr;
    for (Voice& v : voices_) if (!v.on) { slot = &v; break; }
    if (!slot) {   // steal the oldest released voice, else the oldest
        float oldest = -1;
        for (Voice& v : voices_) if ((v.released ? 1000.f : 0.f) + v.age > oldest) { oldest = (v.released ? 1000.f : 0.f) + v.age; slot = &v; }
    }
    Voice& v = *slot;
    v = Voice{};
    v.on = true; v.channel = ch; v.note = note; v.velocity = static_cast<float>(vel) / 127.f;
    v.freq = 440.f * std::pow(2.f, static_cast<float>(note - 69) / 12.f);
    const int prog = program_[static_cast<size_t>(ch)];
    v.family = ch == 9 ? 100 + std::min(note, 99) : prog / 8;
    v.noise = 0;
}

void MidiSynth::noteOff(int ch, int note) {
    for (Voice& v : voices_) if (v.on && !v.released && v.channel == ch && v.note == note) v.released = true;
}

void MidiSynth::handle(const MidiEvent& e) {
    if (e.status == 0xFF) { tempo_ = e.tempo; return; }
    const int ch = e.status & 15;
    switch (e.status >> 4) {
        case 0x9: noteOn(ch, e.a, e.b); break;
        case 0x8: noteOff(ch, e.a); break;
        case 0xC: program_[static_cast<size_t>(ch)] = e.a; break;
        case 0xE: bend_[static_cast<size_t>(ch)] = (static_cast<float>((e.b << 7) | e.a) - 8192.f) / 8192.f * 2.f; break;
        case 0xB:
            if (e.a == 7) volume_[static_cast<size_t>(ch)] = e.b;
            else if (e.a == 11) expression_[static_cast<size_t>(ch)] = e.b;
            else if (e.a == 123 || e.a == 120) for (Voice& v : voices_) if (v.on && v.channel == ch) v.released = true;
            break;
        default: break;
    }
}

void MidiSynth::render(float* out, int frames) {
    if (!file_) return;
    for (int i = 0; i < frames; ++i) {
        if (!finished_) {
            while (next_ < file_->events.size() && file_->events[next_].tick <= tick_) handle(file_->events[next_++]);
            tick_ += file_->division * 1e6 / (double(tempo_) * rate_);
            if (next_ >= file_->events.size() && tick_ > file_->lengthTicks) {
                bool any = false; for (const Voice& v : voices_) any |= v.on;
                if (!any) finished_ = true;
                else for (Voice& v : voices_) v.released = true;
            }
        }
        float l = 0.f;
        for (Voice& v : voices_) {
            if (!v.on) continue;
            const float dt = 1.f / static_cast<float>(rate_);
            v.age += dt;
            float s;
            float attack = 0.005f, decay = 0.f, sustain = 1.f, release = 0.06f;
            const float f = v.freq * std::pow(2.f, bend_[static_cast<size_t>(v.channel)] / 12.f);
            v.phase += f * dt; v.phase -= std::floor(v.phase);
            if (v.family >= 100) {   // percussion by GM note: kick, snare, toms, hats, cymbals, others
                const int n = v.family - 100;
                const float t = v.age;
                if (n == 35 || n == 36) { s = std::sin(2.f * 3.14159265f * (45.f * t + 80.f * (1.f - std::exp(-t * 35.f)) / 35.f)) * std::exp(-t * 10.f); }
                else if (n == 38 || n == 40 || n == 37 || n == 39) { s = nextNoise() * std::exp(-t * 18.f) * 0.8f + std::sin(2.f * 3.14159265f * 180.f * t) * std::exp(-t * 25.f) * 0.4f; }
                else if (n >= 41 && n <= 50) { const float fr = 80.f + static_cast<float>(n - 41) * 18.f; s = std::sin(2.f * 3.14159265f * fr * t) * std::exp(-t * 9.f); }
                else if (n == 42 || n == 44 || n == 46) { s = nextNoise() * std::exp(-t * (n == 46 ? 8.f : 40.f)) * 0.5f; }
                else { s = nextNoise() * std::exp(-t * 6.f) * 0.4f; }
                if (v.age > 0.6f) v.on = false;
                l += s * v.velocity * 0.35f * (volume_[static_cast<size_t>(v.channel)] / 127.f);
                continue;
            }
            const int fam = v.family;
            const float p = v.phase;
            const float tri = 4.f * std::fabs(p - 0.5f) - 1.f, saw = 2.f * p - 1.f, sq = p < 0.5f ? 1.f : -1.f, sine = std::sin(2.f * 3.14159265f * p);
            if (fam < 1) { s = tri * 0.8f + sine * 0.3f; decay = 1.8f; sustain = 0.f; }                            // pianos
            else if (fam < 2) { s = sine; decay = 1.2f; sustain = 0.f; }                                            // chromatic percussion
            else if (fam < 3) { s = sq * 0.45f + sine * 0.4f; sustain = 0.8f; }                                    // organs
            else if (fam < 4) { s = saw * 0.5f + tri * 0.3f; decay = 0.9f; sustain = 0.15f; }                      // guitars
            else if (fam < 5) { s = tri; decay = 1.0f; sustain = 0.4f; }                                           // bass
            else if (fam < 7) { s = saw * 0.45f; attack = 0.12f; sustain = 0.9f; release = 0.15f; }                // strings, ensembles
            else if (fam < 9) { s = sq * 0.4f + saw * 0.2f; attack = 0.03f; sustain = 0.85f; }                     // brass, reeds
            else if (fam < 10) { s = sine; attack = 0.04f; sustain = 0.9f; }                                       // pipes
            else if (fam < 11) { s = sq * 0.5f; attack = 0.01f; sustain = 0.85f; }                                 // synth leads
            else if (fam < 13) { s = saw * 0.35f + sine * 0.2f; attack = 0.2f; sustain = 0.85f; release = 0.25f; }  // pads
            else { s = tri; decay = 1.5f; sustain = 0.2f; }
            float target = v.released ? 0.f : (v.age < attack ? v.age / attack : std::max(sustain, std::exp(-(v.age - attack) * decay)));
            if (decay == 0.f && !v.released && v.age >= attack) target = sustain;
            const float rate = v.released ? dt / release : dt / 0.004f;
            v.env += (target - v.env) * std::min(1.f, rate * (v.released ? 3.f : 1.f));
            if (v.released && v.env < 0.001f) { v.on = false; continue; }
            l += s * v.env * v.velocity * 0.13f * (volume_[static_cast<size_t>(v.channel)] / 127.f) * (expression_[static_cast<size_t>(v.channel)] / 127.f);
        }
        l = std::tanh(l);   // soft limiter
        out[2 * i] += l; out[2 * i + 1] += l;
    }
}

}  // namespace sc2k
