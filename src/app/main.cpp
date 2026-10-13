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
#include "app/title_screen.h"
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
    bool title = false;
    int uiScale = 0;   // --uiscale PCT (percent): overrides the saved High DPI setting
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
        else if (s == "--title") a.title = true;
        else if (s == "--uiscale" && i + 1 < argc) a.uiScale = std::atoi(argv[++i]);
        else if (s == "--size" && i + 2 < argc) { a.width = std::atoi(argv[++i]); a.height = std::atoi(argv[++i]); }
        else a.city = s;
    }
    if (a.city.empty() && a.shot.empty()) a.title = true;   // interactive start without a city: the title screen
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
        // Declared first so it is destroyed last: everything below owns SDL textures / audio streams that must be released before SDL shuts down.
        struct SdlShutdown {
            SDL_Window* window = nullptr;
            SDL_Renderer* renderer = nullptr;
            SDL_Texture* texture = nullptr;
            ~SdlShutdown() {
                if (texture) SDL_DestroyTexture(texture);
                if (renderer) SDL_DestroyRenderer(renderer);
                if (window) SDL_DestroyWindow(window);
                SDL_Quit();
            }
        } sdl;
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
        ui.initNews(args.dir);
        ui.setCityName(city.name());
        TitleScreen titleScreen(args.dir, resources, pal.palette);
        titleScreen.active = args.title;
        SoundBank bank(args.dir);
        Mixer mixer(&bank);
        std::unique_ptr<AudioDevice> audioDevice;
        int musicIndex = 0;
        ui.audio = &mixer;
        {   // the synthesizer chosen in the settings screen (saved between runs)
            std::string err;
            mixer.setBackend(makeMusicBackend(titleScreen.settings, 44100, &err));
            if (!err.empty()) std::fprintf(stderr, "music: %s (using the built-in synthesizer)\n", err.c_str());
        }
        titleScreen.applySettings = [&](const AudioSettings& s) {
            std::string err;
            mixer.setBackend(makeMusicBackend(s, 44100, &err));
            return err.empty() ? "Music: " + mixer.backendName() : "Music: " + mixer.backendName() + " (" + err + ")";
        };
        titleScreen.testMusic = [&] { mixer.playMusic(10001); };
        ui.resize(width, height);
        ui.layers.underground = args.view == "underground";
        ui.layers.powerOverlay = args.view == "power";
        ui.layers.waterOverlay = args.view == "water";

        const bool headless = !args.shot.empty();
        if (headless) { SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"); ui.headless = true; }
        if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
        if (!headless) { audioDevice = std::make_unique<AudioDevice>(mixer); if (!audioDevice->ok()) std::fprintf(stderr, "audio: %s (continuing without sound)\n", SDL_GetError()); }

        SDL_Window*& window = sdl.window;
        SDL_Renderer*& sdlr = sdl.renderer;
        SDL_Surface* target = nullptr;
        if (headless) {
            target = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ARGB8888);
            sdlr = target ? SDL_CreateSoftwareRenderer(target) : nullptr;
        } else {
            window = SDL_CreateWindow("SimCity 2000 (port)", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
            sdlr = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
        }
        if (!sdlr) { std::fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError()); return 1; }

        SDL_Texture*& tex = sdl.texture;
        Framebuffer fb;
        float scaleX = 1.f, scaleY = 1.f;   // window points -> logical UI pixels
        float uiScale = 1.f;
        bool sized = false;
        float wheelAcc = 0.f;
        Uint64 lastWheel = 0;
        Uint64 last = SDL_GetTicks(), thingAcc = 0;
        int shots = 0;
        bool running = true;
        bool ctrl = false;

        auto renderFrame = [&] {
            int pw = width, ph = height;   // physical pixels of the window
            if (window) SDL_GetWindowSizeInPixels(window, &pw, &ph);
            else if (target) { pw = target->w; ph = target->h; }
            // High DPI mode: the whole interface is laid out in pixels / scale and stretched to the window
            const float S = static_cast<float>(args.uiScale > 0 ? args.uiScale : titleScreen.settings.uiScale) / 100.f;
            const int lw = std::max(320, static_cast<int>(pw / S)), lh = std::max(200, static_cast<int>(ph / S));
            uiScale = S;
            if (lw != width || lh != height) {
                width = lw; height = lh;
                ui.resize(lw, lh);
                if (!sized) { sized = true; ui.centerView(sim->state().centerA2 / 2, sim->state().centerB2 / 2); }   // first frame: centre the city in the logical view
                SDL_SetRenderLogicalPresentation(sdlr, lw, lh, SDL_LOGICAL_PRESENTATION_STRETCH);
            }
            if (titleScreen.active) {   // title screen and pickers replace the game view
                SDL_SetRenderDrawColor(sdlr, 8, 12, 24, 255);
                SDL_RenderClear(sdlr);
                titleScreen.draw(sdlr, lw, lh);
                SDL_RenderPresent(sdlr);
                return;
            }
            const int mw = ui.mapW(), mh = ui.mapH();   // the map framebuffer: logical size / zoom magnification
            if (mw != fb.width || mh != fb.height || !tex) {
                fb = Framebuffer(mw, mh);
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(sdlr, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, mw, mh);
                if (tex) SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
            }
            fb.clear(0xFF101820u);
            renderer.draw(fb, sim->world(), sim->state(), cam, ui.layers);
            ui.overlay(fb);
            SDL_UpdateTexture(tex, nullptr, fb.pixels.data(), mw * 4);
            SDL_RenderClear(sdlr);
            const SDL_FRect dst{0.f, 0.f, mw * ui.mapMag(), mh * ui.mapMag()};
            SDL_RenderTexture(sdlr, tex, nullptr, &dst);
            ui.draw(sdlr);
            SDL_RenderPresent(sdlr);
        };

        auto loadGame = [&](const std::string& path) {
            CityFile f = CityFile::load(path);
            sim->replaceCity(f);
            ui.loadCity(f);
            cam.rotation = sim->state().rotation & 3;
            ui.centerView(sim->state().centerA2 / 2, sim->state().centerB2 / 2);
            titleScreen.active = false;
        };
        auto handleTitle = [&] {
            std::string path;
            switch (titleScreen.take(&path)) {
                case TitleScreen::Action::NewCity: titleScreen.active = false; ui.newCityDialog(false); break;
                case TitleScreen::Action::NewMap: titleScreen.active = false; ui.newCityDialog(true); break;
                case TitleScreen::Action::LoadScenario: case TitleScreen::Action::LoadCity: loadGame(path); break;
                case TitleScreen::Action::Quit: running = false; break;
                default: break;
            }
        };
        ui.onTitle = [&] { titleScreen.fromGame = false; titleScreen.show(); };
        ui.onPauseAction = [&](int what) {   // pause menu: load game / settings open the title screen's pickers and come back with Esc or Back
            if (what == 3) { running = false; return; }
            titleScreen.fromGame = true;
            titleScreen.show();
            if (what == 1) titleScreen.openCities(); else if (what == 4) titleScreen.openScenarioList(); else titleScreen.openSettings();
        };
        auto step = [&] {
            if (window) {
                int ww = 1, wh = 1, pw = 1, ph = 1;
                SDL_GetWindowSize(window, &ww, &wh);
                SDL_GetWindowSizeInPixels(window, &pw, &ph);
                scaleX = static_cast<float>(pw) / static_cast<float>(ww) / uiScale;
                scaleY = static_cast<float>(ph) / static_cast<float>(wh) / uiScale;
            }
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (titleScreen.active && (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)) {
                    if (e.type == SDL_EVENT_KEY_DOWN) titleScreen.keyDown(e.key.key);
                    else titleScreen.mouseDown(static_cast<int>(e.button.x * scaleX), static_cast<int>(e.button.y * scaleY), width, height);
                    continue;
                }
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
                    case SDL_EVENT_MOUSE_WHEEL: {   // less sensitive: two wheel notches (or the equivalent trackpad movement) per zoom level; the sum resets after a pause
                        const Uint64 t = SDL_GetTicks();
                        if (t - lastWheel > 300) wheelAcc = 0;
                        lastWheel = t;
                        wheelAcc += e.wheel.y * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.f : 1.f);
                        if (wheelAcc >= 2.f) { ui.wheel(0, 0, 1); wheelAcc = 0; }
                        else if (wheelAcc <= -2.f) { ui.wheel(0, 0, -1); wheelAcc = 0; }
                        break;
                    }
                    default: break;
                }
            }
            const bool* keys = SDL_GetKeyboardState(nullptr);
            const int pan = 12;
            if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) cam.scrollX += pan;
            if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) cam.scrollX -= pan;
            if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) cam.scrollY += pan;
            if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) cam.scrollY -= pan;

            handleTitle();
            const Uint64 now = SDL_GetTicks();
            const int dt = static_cast<int>(now - last);
            last = now;
            if (audioDevice && audioDevice->ok() && !mixer.muted() && mixer.musicEnabled() && !mixer.musicPlaying()) {   // 0x424B60: five tunes in turn
                static const int kTunes[5] = {10001, 10004, 10008, 10012, 10018};
                mixer.playMusic(kTunes[musicIndex++ % 5]);
            }
            cycler.update(dt);
            if (titleScreen.active) { renderFrame(); SDL_Delay(8); return; }
            ui.update(dt);
            const int steps = ui.paused() ? 0 : clock.advance(dt);
            for (int i = 0; i < steps; ++i) sim->tick();
            thingAcc += static_cast<Uint64>(dt);
            while (thingAcc >= 40) { thingAcc -= 40; if (!ui.paused()) sim->thingFrames(1); }
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
                else if (op == "pick") { int a, b, x, y; words >> x >> y; if (ui.pickScreen(x, y, a, b)) std::printf("pick %d %d -> tile %d %d\n", x, y, a, b); else std::printf("pick %d %d -> none\n", x, y); }
                else if (op == "press") { int a, b; words >> a >> b; ui.mouseDown(a, b, 1); }
                else if (op == "zoom") { int z; words >> z; ui.setZoomLevel(z); }
                else if (op == "center") { int a, b; words >> a >> b; ui.centerView(a, b); }
                else if (op == "days") { int d; words >> d; sim->advanceDays(d); }
                else if (op == "rotate") { cam.rotation = (cam.rotation + 1) & 3; ui.centerView(sim->state().centerA2 / 2, sim->state().centerB2 / 2); }
                else if (op == "find") { int id = 0; words >> id; for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) if (sim->world().xbld.at(a, b) == id) { std::printf("building %d at %d %d\n", id, a, b); a = b = 128; } }
                else if (op == "title") titleScreen.show();
                else if (op == "settings") { titleScreen.show(); titleScreen.openSettings(); }
                else if (op == "scenarios") { int k = 0; words >> k; titleScreen.show(); titleScreen.openScenarioList(); titleScreen.select(k); }
                else if (op == "loadscenario") { int k = 0; words >> k; const auto& list = titleScreen.scenarios(); if (k >= 0 && static_cast<size_t>(k) < list.size()) loadGame(list[static_cast<size_t>(k)].path); }
                else if (op == "newcity") { int e = 0, d = 0; words >> e >> d; ui.newCity(e, d); }
                else if (op == "newmap") { int e = 0, d = 0, coast = 0, river = 0; TerrainParams tp; words >> e >> d >> coast >> river >> tp.hills >> tp.water >> tp.trees; tp.coast = coast != 0; tp.river = river != 0; ui.newCity(e, d, &tp); }
                else if (op == "funds") { int f; words >> f; sim->state().treasury = f; }
                else if (op == "key") { std::string k; words >> k; ui.keyDown(keyByName(k), false); }
                else if (op == "budget") { std::string d; words >> d; ui.keyDown(SDLK_B, false); if (!d.empty()) ui.openBudgetDetail(std::atoi(d.c_str())); }
                else if (op == "window") { int w; words >> w; ui.openInfo(w); }
                else if (op == "menu") { std::string n; std::getline(words >> std::ws, n); ui.openMenu(n); }
                else if (!op.empty()) std::fprintf(stderr, "unknown command '%s'\n", op.c_str());
            }
            renderFrame();
            const int ow = target->w, oh = target->h;
            Framebuffer out(ow, oh);
            const auto* px = static_cast<const Uint8*>(target->pixels);
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    uint32_t v;
                    std::memcpy(&v, px + static_cast<size_t>(y) * static_cast<size_t>(target->pitch) + static_cast<size_t>(x) * 4, 4);
                    out.pixels[static_cast<size_t>(y) * static_cast<size_t>(ow) + static_cast<size_t>(x)] = v | 0xFF000000u;
                }
            const bool ok = writePng(args.shot.c_str(), out);
            std::printf("%s funds %d\nmessage: %s\n", ok ? "wrote screenshot" : "write failed", sim->state().treasury, ui.message().c_str());
            return ok ? 0 : 1;
        }

        while (running) step();
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "error: %s\n", ex.what());
        return 1;
    }
    return 0;
}
