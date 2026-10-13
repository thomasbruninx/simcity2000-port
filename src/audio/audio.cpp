#include "audio/audio.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace sc2k {

namespace {
std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}
uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
}  // namespace

Wave SoundBank::parseWave(const std::vector<uint8_t>& d) {
    if (d.size() < 44 || std::memcmp(d.data(), "RIFF", 4) != 0 || std::memcmp(d.data() + 8, "WAVE", 4) != 0) throw std::runtime_error("not a WAV file");
    Wave w;
    int bits = 8, channels = 1, tag = 1;
    size_t pos = 12;
    while (pos + 8 <= d.size()) {
        const uint32_t n = le32(&d[pos + 4]);
        if (std::memcmp(&d[pos], "fmt ", 4) == 0 && n >= 16 && pos + 8 + 16 <= d.size()) {
            tag = le16(&d[pos + 8]); channels = le16(&d[pos + 10]); w.rate = static_cast<int>(le32(&d[pos + 12])); bits = le16(&d[pos + 22]);
        } else if (std::memcmp(&d[pos], "data", 4) == 0) {
            const size_t end = std::min<size_t>(d.size(), pos + 8 + n);
            if (tag != 1 || (bits != 8 && bits != 16)) throw std::runtime_error("only PCM WAV is supported");
            const size_t step = static_cast<size_t>(bits / 8) * static_cast<size_t>(std::max(1, channels));
            for (size_t i = pos + 8; i + step <= end; i += step) {
                float v = bits == 8 ? (static_cast<float>(d[i]) - 128.f) / 128.f : static_cast<float>(static_cast<int16_t>(le16(&d[i]))) / 32768.f;   // first channel only
                w.samples.push_back(v);
            }
            return w;
        }
        pos += 8 + n + (n & 1);
    }
    throw std::runtime_error("WAV without data chunk");
}

const Wave* SoundBank::effect(int id) {
    auto it = waves_.find(id);
    if (it != waves_.end()) return it->second.get();
    if (missing_.count(id)) return nullptr;
    const auto bytes = readFile(dir_ + "/SOUNDS/" + std::to_string(id) + ".WAV");
    try {
        if (bytes.empty()) throw std::runtime_error("missing");
        auto w = std::make_unique<Wave>(parseWave(bytes));
        return (waves_[id] = std::move(w)).get();
    } catch (const std::exception&) { missing_[id] = true; return nullptr; }
}

const MidiFile* SoundBank::music(int id) {
    auto it = midis_.find(id);
    if (it != midis_.end()) return it->second.get();
    if (missing_.count(-id)) return nullptr;
    try {
        auto m = std::make_unique<MidiFile>(MidiFile::load(dir_ + "/SOUNDS/" + std::to_string(id) + ".MID"));
        return (midis_[id] = std::move(m)).get();
    } catch (const std::exception&) { missing_[-id] = true; return nullptr; }
}

void Mixer::playEffect(int id, int repeat) {
    if (!effectsOn_) return;
    const Wave* w = bank_ ? bank_->effect(id) : nullptr;
    if (!w) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (repeat != 0) for (const Voice& v : voices_) if (v.id == id) return;   // a looping sound is not started twice
    for (const Voice& v : voices_) if (v.id == id && v.pos < 0.06 * v.wave->rate) return;   // the same effect just started: many things can fire it in one frame
    if (voices_.size() >= 16) voices_.erase(voices_.begin());
    voices_.push_back({id, w, 0.0, repeat});
}

void Mixer::stopEffect(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id < 0) voices_.clear();
    else voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [&](const Voice& v) { return v.id == id; }), voices_.end());
}

void Mixer::playMusic(int id) {
    if (!musicOn_) return;
    const MidiFile* m = bank_ ? bank_->music(id) : nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    musicId_ = id;
    if (m) synth_->start(m); else synth_->stop();
}

void Mixer::setBackend(std::unique_ptr<MusicBackend> backend) {
    if (!backend) return;
    const MidiFile* running = bank_ && musicId_ ? bank_->music(musicId_) : nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    const bool was = synth_->playing();
    synth_->stop();
    synth_ = std::move(backend);
    if (was && running) synth_->start(running);
}
std::string Mixer::backendName() const { std::lock_guard<std::mutex> lock(mutex_); return synth_->name(); }

void Mixer::stopMusic() { std::lock_guard<std::mutex> lock(mutex_); synth_->stop(); }
bool Mixer::musicPlaying() const { std::lock_guard<std::mutex> lock(mutex_); return synth_->playing(); }
int Mixer::activeEffects() const { std::lock_guard<std::mutex> lock(mutex_); return static_cast<int>(voices_.size()); }

void Mixer::render(float* out, int frames) {
    std::fill(out, out + 2 * frames, 0.f);
    std::lock_guard<std::mutex> lock(mutex_);
    if (muted_) { if (synth_->playing()) { std::vector<float> scratch(static_cast<size_t>(2 * frames), 0.f); synth_->render(scratch.data(), frames); } return; }
    synth_->render(out, frames);
    for (int i = 0; i < 2 * frames; ++i) out[i] *= 0.6f;
    for (Voice& v : voices_) {
        const double step = static_cast<double>(v.wave->rate) / rate_;
        for (int i = 0; i < frames; ++i) {
            const size_t k = static_cast<size_t>(v.pos);
            if (k + 1 >= v.wave->samples.size()) {
                if (v.repeat != 0) { if (v.repeat > 0) --v.repeat; v.pos = 0; } else { v.wave = nullptr; break; }
                continue;
            }
            const float f = static_cast<float>(v.pos - static_cast<double>(k));
            const float s = v.wave->samples[k] * (1.f - f) + v.wave->samples[k + 1] * f;
            out[2 * i] += s * 0.5f; out[2 * i + 1] += s * 0.5f;
            v.pos += step;
        }
    }
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.wave == nullptr; }), voices_.end());
    for (int i = 0; i < 2 * frames; ++i) out[i] = std::max(-1.f, std::min(1.f, out[i]));
}

}  // namespace sc2k
