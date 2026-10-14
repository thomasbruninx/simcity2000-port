# Audio

Port: `src/audio/` (`midi.{h,cpp}`, `audio.{h,cpp}`, `audio_device.cpp`), tool `sc2k-audio`, tests in `tests/test_audio.cpp`.

## What the original has (from SIMCITY.EXE and SOUNDS/)
* **Effects**: `SOUNDS/500.WAV` .. `530.WAV`, 8-bit mono 11025 Hz PCM. The game's sound routine 0x424AA0 takes the file number (0x1F4 = 500): `play(id)`. 0x424AD0 `play(id, repeat)` (-1 = loop, e.g. the terrain sprays 0x1FC), 0x424AF0 `stop(id)`, 0x424B10 stops everything.
* **Music**: `SOUNDS/10000.MID` .. `10018.MID`, SMF format 1, 10 tracks, 192 ticks per quarter note; played through the Windows MIDI mapper (the sound card's synth). Selection (VERIFIED from the code): the game cycles through 10001, 10004, 10008, 10012, 10018 (0x424B60, next tune when one ends); a disaster in play starts 10004 (0x406720 with argument 2); the yearly budget dialog picks one of 10016, 10005, 10002, 10010 at random (0x4736C0, not ported); 10000, 10003, ... are used by menus / title screens (not ported).

## What the port does
* `SoundBank` loads files on demand from the player's game directory; `Mixer` renders interleaved stereo floats (testable without a device): 16 effect voices with linear resampling, loops, plus the MIDI synth; `AudioDevice` feeds an SDL3 audio stream (44.1 kHz, float, stereo). M toggles the sound.
* `MidiSynth` is the port's own small synthesizer: no sound font is shipped or needed. Each General MIDI instrument family gets a simple waveform and envelope (triangle pianos, square organs, saw strings, ...), the drum channel uses sine sweeps and noise. The tunes are recognisable but do not sound like an FM card or a wavetable; this is a deliberate approximation (a SoundFont-based synthesizer could replace it behind the same interface).
* Where sounds are played: the disaster code (all 0x424AA0 calls of the handlers and the driver, alarm 0x208, music change), the tool UI (success / failure of building, zoning, network tools, terrain-spray loop) and `ZoneHooks::playSound / stopSound / playMusic`. The mapping of the tool sounds (0x1F5 success, 0x1F4 failure, 0x1F7 zones, 0x1FC spray loop) is an **inference** from which calls appear next to the success / failure paths of 0x43EBC0 (HYPOTHESIS for the exact meaning of each file); sounds of ships, aircraft, trains, monsters and the other 0x424AA0 call sites (list in `reverse/out/sounds.txt`) are not wired yet.
* `sc2k-audio <dir> music <id> out.wav` and `effect <id> out.wav` render a sound with the port's mixer for listening.

## One sound at a time (0x480700)
The original plays effects through `sndPlaySound` with `SND_ASYNC | SND_MEMORY` (flags 7, 15 with `SND_LOOP` for the repeated ones): there is **one** sound channel, a new sound cuts the running one off (so a demolition, a siren and a horn never overlap), ids outside 500..529 are ignored, and the same sound is not restarted within a few ticks of its start. The port's mixer now keeps a single effect voice the same way (music is separate); before, effects were mixed polyphonically, which made sounds that the original drops or cuts audible.

## Tool sounds (verified against 0x43EBC0 / 0x442040 / 0x442760)
* The **0x1FC loop** ("grinding") belongs to the drags of the *bulldozer menu* (0x442760: demolish, level, raise, lower terrain, de-zone) and of the *terrain editor* tools (0x442040 -> 0x443400 / 0x4432B0 / 0x4442A0 / 0x4423E0); it plays from mouse-down to mouse-up (0x424AD0 .. 0x424AF0). Demolished tiles add the explosion 0x1F8 (0x458690), and 1 in 20 refused demolitions plays 0x200.
* **Roads, rails, pipes, subways, highways and power lines** have no loop: their drag functions (0x449530 road, 0x4498D0 rail, 0x449C90 power line, 0x449F20 pipes, 0x44A0A0 subway, 0x461B80 highway) return a flag and the dispatcher plays **500** (built; **0x202** for a power line) or **0x1F5** (refused) once the drag ends. Zoning plays 0x1F7 (0x1F7 for residential / commercial / industrial, 500 for the port and airport zones) or 0x1F5.
* Landscape: trees and the editor's forest 0x1F7, water and the editor's stream 0x1FF, per tile. Raise / lower / level terrain play only the loop.
