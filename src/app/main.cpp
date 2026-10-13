// SimCity 2000 port: interactive viewer + simulation (SDL3). Loads the player's own game files.
// Usage: sc2k-app <SC2K game dir> [city.sc2] [--screenshot out.png] [--days N]
// Keys: arrows/WASD or mouse drag = scroll, +/- = zoom, R = rotate, Space = pause, 1..5 = speed, F12 = screenshot, Esc = quit.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "render/map_renderer.h"
#include "resources/bmp.h"
#include "save/city.h"
#include "save/error.h"
#include "simulation/clock.h"
#include "simulation/simulation.h"

using namespace sc2k;

namespace {
constexpr const char* kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char* kSpeedNames[6] = {"", "Paused", "Turtle", "Llama", "Cheetah", "African swallow"};
}  // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : (std::getenv("SC2K_DATA_PATH") ? std::getenv("SC2K_DATA_PATH") : "");
    std::string cityPath, shot;
    int shotDays = 0;
    std::string view;
    for (int i = 2; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--screenshot" && i + 1 < argc) shot = argv[++i];
        else if (a == "--view" && i + 1 < argc) view = argv[++i];
        else if (a == "--days" && i + 1 < argc) shotDays = std::atoi(argv[++i]);
        else cityPath = a;
    }
    if (dir.empty()) { std::fprintf(stderr, "usage: sc2k-app <SC2K game dir> [city.sc2] [--screenshot out.png] [--days N]\n"); return 2; }
    if (cityPath.empty()) cityPath = dir + "/CITIES/NYC.SC2";
    try {
        CityFile city = CityFile::load(cityPath);
        auto sim = std::make_unique<Simulation>(city, GameTables::fromExe(dir + "/SIMCITY.EXE"), 1);
        SpriteSet sprites = SpriteSet::load(dir);
        Bmp8 pal = Bmp8::load(dir + "/BITMAPS/PAL_MSTR.BMP");
        PaletteCycler cycler(pal.palette, sim->tables());
        MapRenderer renderer(sim->tables(), sprites, cycler.palette());
        RenderLayers layers;
        layers.underground = view == "underground";
        layers.powerOverlay = view == "power";
        layers.waterOverlay = view == "water";
        Camera cam;
        cam.rotation = sim->state().rotation & 3;
        int width = 1280, height = 800;
        cam.centerOn(sim->state().centerA2 / 2, sim->state().centerB2 / 2, width / 2, height / 2);

        if (!shot.empty()) {   // headless screenshot mode (no window)
            sim->advanceDays(shotDays);
            Framebuffer fb(width, height, 0xFF101820u);
            renderer.draw(fb, sim->world(), sim->state(), cam, layers);
            return writePng(shot.c_str(), fb) ? 0 : 1;
        }

        if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
        SDL_Window* window = SDL_CreateWindow("SimCity 2000 (port)", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        SDL_Renderer* sdlr = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
        if (!sdlr) { std::fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError()); return 1; }
        SDL_Texture* tex = nullptr;
        Framebuffer fb;
        Clock clock;
        clock.speed = GameSpeed::Llama;
        bool running = true, dragging = false;
        Uint64 last = SDL_GetTicks(), thingAcc = 0;
        int shots = 0, blinkMs = 0;
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) running = false;
                else if (e.type == SDL_EVENT_KEY_DOWN) {
                    const SDL_Keycode k = e.key.key;
                    if (k == SDLK_ESCAPE) running = false;
                    else if (k == SDLK_SPACE) clock.speed = clock.speed == GameSpeed::Paused ? GameSpeed::Llama : GameSpeed::Paused;
                    else if (k >= SDLK_1 && k <= SDLK_5) clock.speed = static_cast<GameSpeed>(k - SDLK_1 + 1);
                    else if (k == SDLK_EQUALS || k == SDLK_KP_PLUS) { cam.scrollX -= (width / 2 - cam.scrollX) * 0; cam.zoom = std::min(2, cam.zoom + 1); }
                    else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) cam.zoom = std::max(0, cam.zoom - 1);
                    else if (k == SDLK_R) { cam.rotation = (cam.rotation + 1) & 3; cam.centerOn(sim->state().centerA2 / 2, sim->state().centerB2 / 2, width / 2, height / 2); }
                    else if (k == SDLK_U) layers.underground = !layers.underground;
                    else if (k == SDLK_P) layers.powerOverlay = !layers.powerOverlay;
                    else if (k == SDLK_O) layers.waterOverlay = !layers.waterOverlay;
                    else if (k == SDLK_F12) { char name[64]; std::snprintf(name, sizeof name, "sc2k-%03d.png", shots++); writePng(name, fb); }
                } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) dragging = true;
                else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) dragging = false;
                else if (e.type == SDL_EVENT_MOUSE_MOTION && dragging) { cam.scrollX += static_cast<int>(e.motion.xrel); cam.scrollY += static_cast<int>(e.motion.yrel); }
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
            cycler.update(dt);
            blinkMs += dt; layers.blinkOn = (blinkMs / 400) % 2 == 0;
            const int steps = clock.advance(dt);
            for (int i = 0; i < steps; ++i) sim->tick();
            thingAcc += static_cast<Uint64>(dt);
            while (thingAcc >= 40) { thingAcc -= 40; sim->thingFrames(1); }

            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(window, &pw, &ph);
            if (pw != fb.width || ph != fb.height) {
                fb = Framebuffer(pw, ph);
                if (tex) SDL_DestroyTexture(tex);
                tex = SDL_CreateTexture(sdlr, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, pw, ph);
                width = pw; height = ph;
            }
            fb.clear(0xFF101820u);
            renderer.draw(fb, sim->world(), sim->state(), cam, layers);
            SDL_UpdateTexture(tex, nullptr, fb.pixels.data(), pw * 4);
            SDL_RenderClear(sdlr);
            SDL_RenderTexture(sdlr, tex, nullptr, nullptr);
            const CityState& s = sim->state();
            char hud[200];
            std::snprintf(hud, sizeof hud, "%s %d   Pop %u   Funds $%d   R%d C%d I%d   [%s]", kMonths[s.month()], s.startYear + s.day / 300, s.population10, s.treasury,
                          s.demand[0], s.demand[1], s.demand[2], kSpeedNames[static_cast<int>(clock.speed)]);
            SDL_SetRenderDrawColor(sdlr, 0, 0, 0, 200);
            SDL_FRect bar{0, 0, static_cast<float>(pw), 14};
            SDL_RenderFillRect(sdlr, &bar);
            SDL_SetRenderDrawColor(sdlr, 255, 255, 255, 255);
            SDL_RenderDebugText(sdlr, 4, 3, hud);
            SDL_RenderPresent(sdlr);
            SDL_Delay(8);
        }
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
