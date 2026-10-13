#include "audio/fluid_backend.h"

#include <fluidsynth.h>

#include <algorithm>
#include <vector>

namespace sc2k {

namespace {

class FluidBackend : public MusicBackend {
public:
    FluidBackend(int rate, int volume) : rate_(rate) {
        settings_ = new_fluid_settings();
        fluid_settings_setnum(settings_, "synth.sample-rate", rate);
        fluid_settings_setnum(settings_, "synth.gain", 0.9 * volume / 100.0);
        fluid_settings_setint(settings_, "synth.reverb.active", 1);
        fluid_settings_setint(settings_, "synth.chorus.active", 1);
        synth_ = new_fluid_synth(settings_);
    }
    ~FluidBackend() override {
        if (synth_) delete_fluid_synth(synth_);
        if (settings_) delete_fluid_settings(settings_);
    }
    bool load(const std::string& path) { return synth_ && fluid_synth_sfload(synth_, path.c_str(), 1) != FLUID_FAILED; }

    void start(const MidiFile* f) override {
        file_ = f; next_ = 0; tick_ = 0; tempo_ = 500000; finished_ = f == nullptr; draining_ = false;
        if (synth_) fluid_synth_system_reset(synth_);
    }
    void stop() override { file_ = nullptr; finished_ = true; if (synth_) fluid_synth_all_notes_off(synth_, -1); }
    bool playing() const override { return file_ && !finished_; }
    std::string name() const override { return "FluidSynth"; }

    void render(float* out, int frames) override {
        if (!file_ || !synth_) return;
        buffer_.resize(static_cast<size_t>(frames) * 2);
        int done = 0;
        while (done < frames) {
            // render in slices up to the next MIDI event so timing stays sample accurate
            int slice = frames - done;
            if (!finished_ && !draining_) {
                while (next_ < file_->events.size() && file_->events[next_].tick <= tick_) dispatch(file_->events[next_++]);
                if (next_ < file_->events.size()) {
                    const double ticksPerFrame = file_->division * 1e6 / (double(tempo_) * rate_);
                    const double need = (file_->events[next_].tick - tick_) / ticksPerFrame;
                    slice = std::max(1, std::min(slice, static_cast<int>(need) + 1));
                } else if (tick_ > file_->lengthTicks) {
                    draining_ = true;   // let the notes ring out
                }
            }
            fluid_synth_write_float(synth_, slice, buffer_.data(), 0, 2, buffer_.data(), 1, 2);
            for (int i = 0; i < slice * 2; ++i) out[(done * 2) + i] += buffer_[static_cast<size_t>(i)];
            if (!finished_ && !draining_) tick_ += slice * (file_->division * 1e6 / (double(tempo_) * rate_));
            if (draining_) { drainFrames_ += slice; if (drainFrames_ > rate_ * 2) finished_ = true; }
            done += slice;
            if (!finished_ && !draining_ && next_ >= file_->events.size() && tick_ > file_->lengthTicks) draining_ = true;
        }
    }

private:
    void dispatch(const MidiEvent& e) {
        if (e.status == 0xFF) { tempo_ = e.tempo; return; }
        const int ch = e.status & 15;
        switch (e.status >> 4) {
            case 0x9: if (e.b == 0) fluid_synth_noteoff(synth_, ch, e.a); else fluid_synth_noteon(synth_, ch, e.a, e.b); break;
            case 0x8: fluid_synth_noteoff(synth_, ch, e.a); break;
            case 0xB: fluid_synth_cc(synth_, ch, e.a, e.b); break;
            case 0xC: fluid_synth_program_change(synth_, ch, e.a); break;
            case 0xD: fluid_synth_channel_pressure(synth_, ch, e.a); break;
            case 0xE: fluid_synth_pitch_bend(synth_, ch, (e.b << 7) | e.a); break;
            default: break;
        }
    }
    int rate_;
    fluid_settings_t* settings_ = nullptr;
    fluid_synth_t* synth_ = nullptr;
    const MidiFile* file_ = nullptr;
    size_t next_ = 0;
    double tick_ = 0;
    uint32_t tempo_ = 500000;
    bool finished_ = true, draining_ = false;
    int drainFrames_ = 0;
    std::vector<float> buffer_;
};

}  // namespace

std::unique_ptr<MusicBackend> makeFluidBackend(const std::string& soundfont, int volume, int sampleRate, std::string* error) {
    auto b = std::make_unique<FluidBackend>(sampleRate, volume);
    if (soundfont.empty() || !b->load(soundfont)) {
        if (error) *error = soundfont.empty() ? "no SoundFont selected" : "cannot load SoundFont " + soundfont;
        return nullptr;
    }
    return b;
}

}  // namespace sc2k
