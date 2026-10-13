// Anti-aliased text through FreeType. The newspaper of the original draws "Times New Roman" (GDI CreateFontA(-height)); the port loads the
// same family from the system's fonts at run time (nothing is shipped). Without FreeType or the font file it falls back to SDL's 8x8 debug font.
#pragma once
#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>

namespace sc2k {

class TextRenderer {
public:
    explicit TextRenderer(SDL_Renderer* r);
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    bool hasFont() const { return face_ != nullptr; }
    int width(const std::string& text, int px, bool bold = false);                                        // advance of the line at em height `px`
    void draw(const std::string& text, int x, int y, int px, Uint8 cr, Uint8 cg, Uint8 cb, bool bold = false);   // (x, y) = top-left of the line box (height ~ 1.15 px)

private:
    struct Entry { SDL_Texture* tex = nullptr; int w = 0, h = 0; };
    SDL_Renderer* r_;
    void* lib_ = nullptr;
    void* face_ = nullptr;
    void* faceBold_ = nullptr;   // Times New Roman Bold (the neighbors window); the regular face when missing
    void* faceFor(bool bold) const { return bold && faceBold_ ? faceBold_ : face_; }
    std::unordered_map<std::string, Entry> cache_;
    std::unordered_map<std::string, int> widths_;
    Entry& render(const std::string& text, int px, bool bold);
};

}  // namespace sc2k
