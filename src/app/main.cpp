// SimCity 2000 port: interactive game (SDL3). Loads the player's own game files.
// Usage: sc2k-app <SC2K game dir> [city.sc2] [--screenshot out.png] [--days N] [--view underground|power|water] [--size W H] [--do "cmd; cmd; ..."]
// Keys and mouse: see the hint line at the bottom of the window. Headless mode (--screenshot) renders one frame with the real UI into a PNG;
// --do runs scripted actions first: tool NAME | menu NAME | drag a0 b0 a1 b1 | click a b | zoom N | center a b | days N | rotate | funds N | key NAME.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>

#include "app/game_ui.h"
#include "audio/audio.h"
#include "render/map_renderer.h"
#include "resources/bmp.h"
#include "resources/pe_resources.h"
#include "save/city.h"
#include "save/error.h"
#include "simulation/clock.h"
#include "simulation/simulation.h"

using namespace sc2k;

namespace {
struct Args {
    std::string dir, city, shot, view, script;
    int days = 0, width = 1280, height = 800;
};

Args parse(int argc, char** argv) {
    Args a;
    a.dir = argc > 1 ? argv[1] : (std::getenv("SC2K_DATA_PATH") ? std::getenv("SC2K_DATA_PATH") : "");
    for (int i = 2; i < argc; ++i) {
        const std::string s = argv[i];
        if (s == "--screenshot" && i + 1 < argc) a.shot = argv[++i];
        else if (s == "--days" && i + 1 < argc) a.days = std::atoi(argv[++i]);
        else if (s == "--view" && i + 1 < argc) a.view = argv[++i];
        else if (s == "--do" && i + 1 < argc) a.script = argv[++i];
        else if (s == "--size" && i + 2 < argc) { a.width = std::atoi(argv[++i]); a.height = std::atoi(argv[++i]); }
        else a.city = s;
    }
    return a;
}

SDL_Keycode keyByName(const std::string& n) {
    if (n.size() == 1) return static_cast<SDL_Keycode>(std::tolower(n[0]));
    if (n == "escape") return SDLK_ESCAPE;
    if (n == "space") return SDLK_SPACE;
    return SDLK_UNKNOWN;
}
}  // namespace

int main(int argc, char** argv) {
    const Args args = parse(argc, argv);
    if (args.dir.empty()) { std::fprintf(stderr, "usage: sc2k-app <SC2K game dir> [city.sc2] [--screenshot out.png] [--do \"...\"]\n"); return 2; }
    try {
        const std::string cityPath = args.city.empty() ? args.dir + "/CITIES/NYC.SC2" : args.city;
        CityFile city = CityFile::load(cityPath);
        auto sim = std::make_unique<Simulation>(city, GameTables::fromExe(args.dir + "/SIMCITY.EXE"), 1);
        const PeImage exe = PeImage::load(args.dir + "/SIMCITY.EXE");
        const PeResources resources(exe);
        SpriteSet sprites = SpriteSet::load(args.dir);
        Bmp8 pal = Bmp8::load(args.dir + "/BITMAPS/PAL_MSTR.BMP");
        PaletteCycler cycler(pal.palette, sim->tables());
        MapRenderer renderer(sim->tables(), sprites, cycler.palette());
        Camera cam;
        cam.rotation = sim->state().rotation & 3;
        int width = args.width, height = args.height;
        cam.centerOn(sim->state().centerA2 / 2, sim->state().centerB2 / 2, width / 2, height / 2);
        Clock clock;
        clock.speed = GameSpeed::Llama;
        GameUi ui(*sim, renderer, cam, clock, resources, city);
        sim->setHooks(ui);
        ui.defaultMapPath = args.dir + "/DEFAULT.SC2";
        SoundBank bank(args.dir);
        Mixer mixer(&bank);
        std::unique_ptr<AudioDevice> audioDevice;
        int musicIndex = 0;
        ui.audio = &mixer;
        ui.resize(width, height);
        ui.layers.underground = args.view == "underground";
        ui.layers.powerOverlay = args.view == "power";
        ui.layers.waterOverlay = args.view == "water";

        const bool headless = !args.shot.empty();
        if (headless) { SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"); ui.headless = true; }
        if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
        if (!headless) { audioDevice = std::make_unique<AudioDevice>(mixer); if (!audioDevice->ok()) std::fprintf(stderr, "audio: %s (continuing without sound)\n", SDL_GetError()); }

        SDL_Window* window = nullptr;
        SDL_Renderer* sdlr = nullptr;
        SDL_Surface* target = nullptr;
        if (headless) {
            target = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
            sdlr = target ? SDL_CreateSoftwareRenderer(target) : nullptr;
        } else {
            window = SDL_CreateWindow("SimCity 2000 (port)", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
            sdlr = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
        }
        if (!sdlr) { std::fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError()); return 1; }

        SDL_Texture* tex = nullptr;
        Framebuffer fb;
        float scaleX = 1.f, scaleY = 1.f;   // window points -> framebuffer pixels
        Uint64 last = SDL_GetTicks(), thingAcc = 0;
        int shots = 0;
        bool running = true;
        bool ctrl = false;

        auto renderFrame = [&] {
            int pw = width, ph = height;
            if (window) SDL_GetWindowSizeInPixels(window, &pw, &ph);
            if (pw != fb.width || ph != fb.height || !tex) {
                fb = Framebuffer(pw, ph);
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(sdlr, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, pw, ph);
                width = pw; height = ph;
                ui.resize(pw, ph);
            }
            fb.clear(0xFF101820u);
            renderer.draw(fb, sim->world(), sim->state(), cam, ui.layers);
            ui.overlay(fb);
            SDL_UpdateTexture(tex, nullptr, fb.pixels.data(), pw * 4);
            SDL_RenderClear(sdlr);
            SDL_RenderTexture(sdlr, tex, nullptr, nullptr);
            ui.draw(sdlr);
            SDL_RenderPresent(sdlr);
        };

        auto step = [&] {
            if (window) {
                int ww = 1, wh = 1, pw = 1, ph = 1;
                SDL_GetWindowSize(window, &ww, &wh);
                SDL_GetWindowSizeInPixels(window, &pw, &ph);
                scaleX = static_cast<float>(pw) / static_cast<float>(ww);
                scaleY = static_cast<float>(ph) / static_cast<float>(wh);
            }
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                switch (e.type) {
                    case SDL_EVENT_QUIT: running = false; break;
                    case SDL_EVENT_KEY_DOWN:
                        ctrl = (e.key.mod & SDL_KMOD_CTRL) != 0 || (e.key.mod & SDL_KMOD_GUI) != 0;
                        ui.setShift((e.key.mod & SDL_KMOD_SHIFT) != 0);
                        if (e.key.key == SDLK_F12) { char name[64]; std::snprintf(name, sizeof name, "sc2k-%03d.png", shots++); writePng(name, fb); }
                        else if (!ui.keyDown(e.key.key, ctrl)) running = false;
                        break;
                    case SDL_EVENT_TEXT_INPUT: ui.textInput(e.text.text); break;
                    case SDL_EVENT_KEY_UP: ui.setShift((e.key.mod & SDL_KMOD_SHIFT) != 0); break;
                    case SDL_EVENT_MOUSE_BUTTON_DOWN: ui.mouseDown(static_cast<int>(e.button.x * scaleX), static_cast<int>(e.button.y * scaleY), e.button.button); break;
                    case SDL_EVENT_MOUSE_BUTTON_UP: ui.mouseUp(static_cast<int>(e.button.x * scaleX), static_cast<int>(e.button.y * scaleY), e.button.button); break;
                    case SDL_EVENT_MOUSE_MOTION: ui.mouseMove(static_cast<int>(e.motion.x * scaleX), static_cast<int>(e.motion.y * scaleY)); break;
                    case SDL_EVENT_MOUSE_WHEEL: ui.wheel(0, 0, e.wheel.y > 0 ? 1 : (e.wheel.y < 0 ? -1 : 0)); break;
                    default: break;
                }
            }
            const bool* keys = SDL_GetKeyboardState(nullptr);
            const int pan = 12;
            if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) cam.scrollX += pan;
            if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) cam.scrollX -= pan;
            if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) cam.scrollY += pan;
            if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) cam.scrollY -= pan;

            const Uint64 now = SDL_GetTicks();
            const int dt = static_cast<int>(now - last);
            last = now;
            if (audioDevice && audioDevice->ok() && !mixer.muted() && !mixer.musicPlaying()) {   // 0x424B60: five tunes in turn
                static const int kTunes[5] = {10001, 10004, 10008, 10012, 10018};
                mixer.playMusic(kTunes[musicIndex++ % 5]);
            }
            cycler.update(dt);
            ui.update(dt);
            const int steps = clock.advance(dt);
            for (int i = 0; i < steps; ++i) sim->tick();
            thingAcc += static_cast<Uint64>(dt);
            while (thingAcc >= 40) { thingAcc -= 40; sim->thingFrames(1); }
            renderFrame();
            SDL_Delay(8);
        };
        ui.pump = [&] { const GameSpeed saved = clock.speed; clock.speed = GameSpeed::Paused; step(); clock.speed = saved; return running; };

        if (headless) {
            sim->advanceDays(args.days);
            renderFrame();   // creates the framebuffer / texture
            std::stringstream script(args.script);
            std::string cmd;
            while (std::getline(script, cmd, ';')) {
                std::stringstream words(cmd);
                std::string op;
                words >> op;
                if (op == "tool") { std::string name; std::getline(words >> std::ws, name); if (!ui.selectTool(name)) std::fprintf(stderr, "tool '%s' not available\n", name.c_str()); }
                else if (op == "drag") { int a0, b0, a1, b1; words >> a0 >> b0 >> a1 >> b1; ui.dragTool(a0, b0, a1, b1); }
                else if (op == "sign") { int a, b; words >> a >> b; std::string t; std::getline(words >> std::ws, t); ui.signTool(a, b, t); }
                else if (op == "click") { int a, b; words >> a >> b; ui.dragTool(a, b, a, b); }
                else if (op == "zoom") { int z; words >> z; cam.zoom = std::clamp(z, 0, 2); }
                else if (op == "center") { int a, b; words >> a >> b; cam.centerOn(a, b, width / 2, height / 2); }
                else if (op == "days") { int d; words >> d; sim->advanceDays(d); }
                else if (op == "rotate") { cam.rotation = (cam.rotation + 1) & 3; cam.centerOn(sim->state().centerA2 / 2, sim->state().centerB2 / 2, width / 2, height / 2); }
                else if (op == "find") { int id = 0; words >> id; for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) if (sim->world().xbld.at(a, b) == id) { std::printf("building %d at %d %d\n", id, a, b); a = b = 128; } }
                else if (op == "newcity") { int e = 0, d = 0; words >> e >> d; ui.newCity(e, d); }
                else if (op == "funds") { int f; words >> f; sim->state().treasury = f; }
                else if (op == "key") { std::string k; words >> k; ui.keyDown(keyByName(k), false); }
                else if (op == "budget") { std::string d; words >> d; ui.keyDown(SDLK_B, false); if (!d.empty()) ui.openBudgetDetail(std::atoi(d.c_str())); }
                else if (op == "window") { int w; words >> w; ui.openInfo(w); }
                else if (op == "menu") { std::string n; std::getline(words >> std::ws, n); ui.openMenu(n); }
                else if (!op.empty()) std::fprintf(stderr, "unknown command '%s'\n", op.c_str());
            }
            renderFrame();
            Framebuffer out(width, height);
            const auto* px = static_cast<const Uint8*>(target->pixels);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    uint32_t v;
                    std::memcpy(&v, px + static_cast<size_t>(y) * static_cast<size_t>(target->pitch) + static_cast<size_t>(x) * 4, 4);
                    out.pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = v | 0xFF000000u;
                }
            const bool ok = writePng(args.shot.c_str(), out);
            std::printf("%s funds %d\nmessage: %s\n", ok ? "wrote screenshot" : "write failed", sim->state().treasury, ui.message().c_str());
            return ok ? 0 : 1;
        }

        while (running) step();
        if (tex) SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(sdlr);
        SDL_DestroyWindow(window);
        SDL_Quit();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "error: %s\n", ex.what());
        return 1;
    }
    return 0;
}
