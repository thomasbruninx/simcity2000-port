# Music synthesizer settings

* **Built-in synthesizer** (`MidiSynth`, docs/audio.md): small additive synth, always available.
* **FluidSynth** (`src/audio/fluid_backend.cpp`): renders the game's MIDI files with a SoundFont (any `.sf2`; `resources/GeneralUser-GS.sf2` is found automatically). Optional at build time: CMake enables it when `pkg-config fluidsynth` finds the library (`brew install fluid-synth`; the library is LGPL and linked dynamically). Without it the settings screen greys the option out.
* **Settings screen**: title screen -> *Settings* (Ctrl+O in the game returns to the title screen). Choose the synthesizer, the SoundFont (all `.sf2` files in `./resources`, `../resources`, `../../resources`, the game folder and `$SC2K_RESOURCES`), the music volume and press *Play test tune*; changes apply immediately and are saved to `~/Library/Application Support/sc2k-port/settings.ini` (`$XDG_CONFIG_HOME/sc2k-port` on Linux, `$SC2K_SETTINGS_DIR` overrides). On a first run without a settings file FluidSynth is selected when it and a SoundFont are available.
* If the SoundFont cannot be loaded the game falls back to the built-in synthesizer and reports it (status line / stderr).
* `sc2k-audio` renders with the saved choice; `SYNTH=builtin|fluid SOUNDFONT=path sc2k-audio <dir> music 10001 out.wav` overrides it.
