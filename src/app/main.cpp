// SimCity 2000 port: interactive game (SDL3). Loads the player's own game files.
// Usage: sc2k-app <SC2K game dir> [city.sc2] [--screenshot out.png] [--days N] [--view underground|power|water] [--size W H] [--do "cmd; cmd; ..."]
// Keys and mouse: see the hint line at the bottom of the window. Headless mode (--screenshot) renders one frame with the real UI into a PNG;
// --do runs scripted actions first: tool NAME | menu NAME | drag a0 b0 a1 b1 | click a b | zoom N | center a b | days N | rotate | funds N | key NAME.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>
#include <memory>
#include <sstream>
#include <string>

#include "app/game_ui.h"
#include "app/scurk/editors.h"
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

bool writeBmp24(const std::string& path, const Framebuffer& fb) {
    const int w = fb.width, h = fb.height, row = (w * 3 + 3) & ~3;
    std::vector<uint8_t> out(54 + static_cast<size_t>(row) * h, 0);
    auto put32 = [&](size_t o, uint32_t v) { for (int i = 0; i < 4; ++i) out[o + i] = static_cast<uint8_t>(v >> (8 * i)); };
    out[0] = 'B'; out[1] = 'M';
    put32(2, static_cast<uint32_t>(out.size())); put32(10, 54); put32(14, 40); put32(18, static_cast<uint32_t>(w)); put32(22, static_cast<uint32_t>(h));
    out[26] = 1; out[28] = 24; put32(34, static_cast<uint32_t>(row * h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t p = fb.pixels[static_cast<size_t>(y) * w + x];
            uint8_t* d = &out[54 + static_cast<size_t>(h - 1 - y) * row + x * 3];
            d[0] = p & 0xFF; d[1] = (p >> 8) & 0xFF; d[2] = (p >> 16) & 0xFF;
        }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}
constexpr Uint64 kThingFrameMs = 100;   // animation frame interval of ships, planes, vehicles, smoke ... (10 frames per second)

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
        std::unique_ptr<scurk::ResourceEditors> editors;   // the SCURK port, created when first opened
        auto editorsActive = [&] { return editors && editors->active; };
        int presentedW = 0, presentedH = 0, lastUiW = 0, lastUiH = 0;
        float wheelAcc = 0.f;
        Uint64 lastWheel = 0;
        Uint64 last = SDL_GetTicks(), thingAcc = 0;
        int shots = 0;
        bool running = true;
        bool ctrl = false;

        std::function<void()> setupCityBridge, blankCity;
        auto renderFrame = [&] {
            int pw = width, ph = height;   // physical pixels of the window
            if (window) SDL_GetWindowSizeInPixels(window, &pw, &ph);
            else if (target) { pw = target->w; ph = target->h; }
            // High DPI mode: the whole interface is laid out in pixels / scale and stretched to the window
            const float S = static_cast<float>(args.uiScale > 0 ? args.uiScale : titleScreen.settings.uiScale) / 100.f;
            const int lw = std::max(320, static_cast<int>(pw / S)), lh = std::max(200, static_cast<int>(ph / S));
            uiScale = S;
            if (lw != presentedW || lh != presentedH) {   // (the first frame always sets it: the window's point size can equal the logical size by chance, e.g. 2560 px at x2)
                presentedW = lw; presentedH = lh;
                width = lw; height = lh;
                ui.resize(lw, lh);
                if (!sized) { sized = true; ui.centerView(sim->state().centerA2 / 2, sim->state().centerB2 / 2); }   // first frame: centre the city in the logical view
                SDL_SetRenderLogicalPresentation(sdlr, lw, lh, SDL_LOGICAL_PRESENTATION_STRETCH);
            }
            {   // Place & Print: the SCURK status line sits below the game's view
                const int uiH = lh - (editorsActive() && ui.cityEditing() ? 20 : 0);
                if (uiH != lastUiH || lw != lastUiW) { ui.resize(lw, uiH); lastUiH = uiH; lastUiW = lw; }
            }
            if (editorsActive() && !editors->drawsOwnBackground()) {   // the resource editors replace everything
                SDL_SetRenderDrawColor(sdlr, 8, 12, 24, 255);
                SDL_RenderClear(sdlr);
                editors->draw(sdlr, lw, lh);
                SDL_RenderPresent(sdlr);
                return;
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
            if (editorsActive()) editors->draw(sdlr, lw, lh);   // Place & Print: the SCURK menu and status line over the map view
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
        // Place & Print: the editor runs the application's own map view in its editing mode
        auto applyWorking = [&] {
            sprites.clearOverrides();
            editors->env().working.applyTo(sprites);
        };
        blankCity = [&] {   // File > New City / opening Place & Print: a blank terrain
            try { loadGame(args.dir + "/DEFAULT.SC2"); ui.resetCityPath(); ui.setCityName("Untitled"); } catch (const std::exception&) {}
        };
        setupCityBridge = [&] {
            scurk::CityBridge& b = editors->city;
            b.enter = [&, applyWorking] { blankCity(); ui.setCityEdit(true); applyWorking(); };
            b.reserve = [&](ui::Rect rc) { ui.setReserved(rc); };
            b.selectObject = [&](int id) { ui.setPlaceObject(id); };
            b.selectedObject = [&] { return ui.placeObject(); };
            b.leave = [&] { ui.setCityEdit(false); sprites.clearOverrides(); };
            b.draw = [&](SDL_Renderer*) {};
            b.mouseDown = [&](int x, int y, int bt) { ui.mouseDown(x, y, bt); };
            b.mouseUp = [&](int x, int y, int bt) { ui.mouseUp(x, y, bt); };
            b.mouseMove = [&](int x, int y) { ui.mouseMove(x, y); };
            b.wheel = [&](int dy) { ui.wheel(ui.mapW(), 0, dy); };
            b.key = [&](SDL_Keycode k, bool ctrl) {
                if (k == SDLK_ESCAPE) return ui.clearSelection();
                if (ctrl) return false;
                ui.keyDown(k, false);
                return true;
            };
            b.loadCity = [&](const std::string& p) {
                try { loadGame(p); titleScreen.active = false; ui.setCityEdit(true); return true; } catch (const std::exception&) { return false; }
            };
            b.saveCity = [&](const std::string& p) { return ui.saveCity(p); };
            b.newCity = [&] { blankCity(); ui.setCityEdit(true); };
            b.cityPath = [&] { return ui.cityPath(); };
            b.cityName = [&] { return ui.cityName(); };
            b.applyTileSet = [applyWorking](const std::string&) { applyWorking(); };
            b.exportCity = [&](const std::string& p) {   // the whole map at the smallest zoom, as a 24-bit BMP ("small view only")
                if (!ui.smallView()) return false;
                Camera c0; c0.zoom = 0; c0.rotation = 0;
                Framebuffer shot(1032, 520, 0xFF000000u);
                c0.centerOn(64, 64, 516, 260);
                RenderLayers lay; lay.powerOverlay = lay.waterOverlay = false;
                renderer.draw(shot, sim->world(), sim->state(), c0, lay);
                return writeBmp24(p, shot);
            };
        };
        auto handleTitle = [&] {
            std::string path;
            switch (titleScreen.take(&path)) {
                case TitleScreen::Action::NewCity: titleScreen.active = false; ui.newCityDialog(false); break;
                case TitleScreen::Action::NewMap: titleScreen.active = false; ui.newCityDialog(true); break;
                case TitleScreen::Action::LoadScenario: case TitleScreen::Action::LoadCity: loadGame(path); break;
                case TitleScreen::Action::OpenEditor: {
                    if (!editors) {
                        editors = std::make_unique<scurk::ResourceEditors>(args.dir, pal.palette, sprites, &sim->tables());
                        editors->onExit = [&] { titleScreen.fromGame = false; titleScreen.show(); if (window) SDL_StopTextInput(window); };
                        setupCityBridge();
                    }
                    static const scurk::ResourceEditors::Which kWhich[3] = {scurk::ResourceEditors::Which::PlaceAndPrint, scurk::ResourceEditors::Which::PickAndCopy, scurk::ResourceEditors::Which::PaintTheTown};
                    titleScreen.active = false;
                    editors->open(kWhich[titleScreen.editor % 3]);
                    if (!editors->active) titleScreen.show();
                    else if (window) SDL_StartTextInput(window);
                    break;
                }
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
                if (editorsActive() && e.type != SDL_EVENT_QUIT) {   // the resource editors take every input event
                    const int ex = static_cast<int>(e.button.x * scaleX), ey = static_cast<int>(e.button.y * scaleY);
                    switch (e.type) {
                        case SDL_EVENT_KEY_DOWN: editors->keyDown(e.key.key, (e.key.mod & SDL_KMOD_CTRL) != 0 || (e.key.mod & SDL_KMOD_GUI) != 0); break;
                        case SDL_EVENT_TEXT_INPUT: editors->textInput(e.text.text); break;
                        case SDL_EVENT_MOUSE_BUTTON_DOWN: editors->mouseDown(ex, ey, e.button.button, width, height); break;
                        case SDL_EVENT_MOUSE_BUTTON_UP: editors->mouseUp(ex, ey, e.button.button); break;
                        case SDL_EVENT_MOUSE_MOTION: editors->mouseMove(static_cast<int>(e.motion.x * scaleX), static_cast<int>(e.motion.y * scaleY)); break;
                        case SDL_EVENT_MOUSE_WHEEL: editors->wheel(0, 0, e.wheel.y > 0 ? 1 : (e.wheel.y < 0 ? -1 : 0)); break;
                        default: break;
                    }
                    continue;
                }
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
                        wheelAcc += e.wheel.y;
                        if (wheelAcc >= 2.f) { ui.wheel(0, 0, 1); wheelAcc = 0; }
                        else if (wheelAcc <= -2.f) { ui.wheel(0, 0, -1); wheelAcc = 0; }
                        break;
                    }
                    default: break;
                }
            }
            const bool* keys = SDL_GetKeyboardState(nullptr);
            const int pan = 12;
            if (!ui.inputCaptured() && !titleScreen.active && (!editorsActive() || ui.cityEditing())) {
                if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) cam.scrollX += pan;
                if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) cam.scrollX -= pan;
                if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) cam.scrollY += pan;
                if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) cam.scrollY -= pan;
            }

            handleTitle();
            const Uint64 now = SDL_GetTicks();
            const int dt = static_cast<int>(now - last);
            last = now;
            if (audioDevice && audioDevice->ok() && !mixer.muted() && mixer.musicEnabled() && !mixer.musicPlaying()) {   // 0x424B60: five tunes in turn
                static const int kTunes[5] = {10001, 10004, 10008, 10012, 10018};
                mixer.playMusic(kTunes[musicIndex++ % 5]);
            }
            cycler.update(dt);
            if (editorsActive()) { editors->update(dt); renderFrame(); SDL_Delay(8); return; }
            if (titleScreen.active) { renderFrame(); SDL_Delay(8); return; }
            ui.update(dt);
            const int steps = ui.paused() ? 0 : clock.advance(dt);
            for (int i = 0; i < steps; ++i) sim->tick();
            thingAcc += static_cast<Uint64>(dt);
            while (thingAcc >= kThingFrameMs) { thingAcc -= kThingFrameMs; if (!ui.paused()) sim->thingFrames(1); }   // the original ran its frame update every 30-45 ms on 1996 hardware, which looks too fast today
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
                else if (op == "tile") { int a, b; words >> a >> b; std::printf("tile %d %d: bld %02x bit %02x zon %02x txt %02x\n", a, b, sim->world().xbld.at(a, b), sim->world().xbit.at(a, b), sim->world().xzon.at(a, b), sim->world().xtxt.at(a, b)); }
                else if (op == "modal") { std::string t; std::getline(words >> std::ws, t); ui.showTestModal(t, t.rfind("field", 0) == 0); }
                else if (op == "browser") ui.showBrowser();
                else if (op == "editor") {   // headless: open a resource editor (0 Place & Print, 1 Pick & Copy, 2 Paint The Town)
                    int n = 1; words >> n;
                    if (!editors) { editors = std::make_unique<scurk::ResourceEditors>(args.dir, pal.palette, sprites, &sim->tables()); setupCityBridge(); }
                    static const scurk::ResourceEditors::Which kW[3] = {scurk::ResourceEditors::Which::PlaceAndPrint, scurk::ResourceEditors::Which::PickAndCopy, scurk::ResourceEditors::Which::PaintTheTown};
                    titleScreen.active = false;
                    editors->open(kW[n % 3]);
                }
                else if (op == "bridges") { int n = 0; for (int a = 0; a < 128; ++a) for (int b = 0; b < 128; ++b) { const int id = sim->world().xbld.at(a, b); if ((id == 0x49 || id == 0x4A) && (sim->world().xbit.at(a, b) & 0x1) == 0 && sim->world().xter.at(a, b) >= 0x30) ++n; } std::printf("bridge tiles over water: %d\n", n); }
                else if (op == "frame") renderFrame();   // draw once (editors lay out during drawing)
                else if (op == "edrag") { int a, b, c2, d2; words >> a >> b >> c2 >> d2; if (editorsActive()) { editors->mouseDown(a, b, 1, width, height); editors->mouseMove(c2, d2); editors->mouseUp(c2, d2, 1); } }
                else if (op == "mapedit") ui.enterMapEditor();
                else if (op == "press") { int a, b; words >> a >> b; if (editorsActive()) { editors->mouseDown(a, b, 1, width, height); editors->mouseUp(a, b, 1); } else ui.mouseDown(a, b, 1); }
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
