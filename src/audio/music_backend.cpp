#include "audio/music_backend.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>

#ifdef SC2K_HAVE_FLUIDSYNTH
#include "audio/fluid_backend.h"
#endif

namespace sc2k {

namespace {
bool isDir(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
void mkdirs(const std::string& p) {
    for (size_t i = 1; i < p.size(); ++i) if (p[i] == '/') mkdir(p.substr(0, i).c_str(), 0755);
    mkdir(p.c_str(), 0755);
}
}  // namespace

std::string AudioSettings::settingsPath() {
    const char* home = std::getenv("HOME");
    std::string base;
#ifdef __APPLE__
    base = std::string(home ? home : ".") + "/Library/Application Support";
#else
    if (const char* x = std::getenv("XDG_CONFIG_HOME")) base = x; else base = std::string(home ? home : ".") + "/.config";
#endif
    if (const char* o = std::getenv("SC2K_SETTINGS_DIR")) base = o;
    return base + "/sc2k-port/settings.ini";
}

AudioSettings AudioSettings::load() {
    AudioSettings s;
    std::ifstream in(settingsPath());
    if (!in && fluidSynthAvailable() && !discoverSoundfonts().empty()) s.synth = Synth::FluidSynth;   // first run: better sound when a SoundFont is at hand
    std::string line;
    while (std::getline(in, line)) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "synth") s.synth = v == "fluidsynth" ? Synth::FluidSynth : Synth::Builtin;
        else if (k == "soundfont") s.soundfont = v;
        else if (k == "volume") s.volume = std::clamp(std::atoi(v.c_str()), 0, 100);
    }
    return s;
}

bool AudioSettings::save() const {
    const std::string path = settingsPath();
    mkdirs(path.substr(0, path.rfind('/')));
    std::ofstream out(path);
    if (!out) return false;
    out << "synth=" << (synth == Synth::FluidSynth ? "fluidsynth" : "builtin") << "\nsoundfont=" << soundfont << "\nvolume=" << volume << "\n";
    return static_cast<bool>(out);
}

std::vector<std::string> AudioSettings::discoverSoundfonts(const std::string& extraDir) {
    std::vector<std::string> dirs = {"resources", "../resources", "../../resources", extraDir};
    if (const char* x = std::getenv("SC2K_RESOURCES")) dirs.push_back(x);
    std::vector<std::string> found;
    for (const std::string& d : dirs) {
        if (d.empty() || !isDir(d)) continue;
        if (DIR* dir = opendir(d.c_str())) {
            while (const dirent* e = readdir(dir)) {
                std::string n = e->d_name;
                std::string low = n;
                std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return std::tolower(c); });
                if (low.size() > 4 && low.compare(low.size() - 4, 4, ".sf2") == 0) {
                    char real[4096];
                    const std::string full = d + "/" + n;
                    found.push_back(realpath(full.c_str(), real) ? std::string(real) : full);
                }
            }
            closedir(dir);
        }
    }
    std::sort(found.begin(), found.end());
    found.erase(std::unique(found.begin(), found.end()), found.end());
    return found;
}

bool fluidSynthAvailable() {
#ifdef SC2K_HAVE_FLUIDSYNTH
    return true;
#else
    return false;
#endif
}

std::unique_ptr<MusicBackend> makeMusicBackend(const AudioSettings& s, int sampleRate, std::string* error) {
#ifdef SC2K_HAVE_FLUIDSYNTH
    if (s.synth == AudioSettings::Synth::FluidSynth) {
        std::string sf = s.soundfont;
        if (sf.empty()) { const auto all = AudioSettings::discoverSoundfonts(); if (!all.empty()) sf = all.front(); }
        if (auto b = makeFluidBackend(sf, s.volume, sampleRate, error)) return b;
        return std::make_unique<MidiSynth>(sampleRate);
    }
#else
    if (s.synth == AudioSettings::Synth::FluidSynth && error) *error = "this build has no FluidSynth support";
#endif
    return std::make_unique<MidiSynth>(sampleRate);
}

}  // namespace sc2k
