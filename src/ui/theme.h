// The colours of the port's interface in one place (RGB). Widgets read them from here, so a different look is one edit.
#pragma once
#include <cstdint>

namespace sc2k::ui {

struct Color { uint8_t r, g, b; };

namespace theme {
constexpr Color kPanel{28, 32, 44}, kPanelBorder{200, 200, 220}, kTitle{255, 255, 190}, kText{235, 235, 235}, kTextDim{150, 160, 180}, kTextWarn{255, 170, 170};
constexpr Color kField{12, 14, 20}, kFieldText{255, 255, 255};
constexpr Color kButton{40, 56, 96}, kButtonOn{70, 100, 150}, kButtonBorder{190, 200, 230}, kButtonText{255, 255, 220}, kButtonDisabled{60, 64, 76};
constexpr Color kRow{20, 28, 44}, kRowSelected{60, 90, 140}, kRowHover{44, 56, 84};
constexpr Color kTrack{80, 90, 120}, kKnob{230, 230, 140};
constexpr Color kBackdrop{0, 0, 0};          // drawn with alpha 120..140 behind dialogs
constexpr Color kPaper{250, 246, 210}, kPaperBorder{70, 50, 20}, kPaperText{40, 20, 0}, kPaperDim{120, 100, 80};   // the query popup
}  // namespace theme

}  // namespace sc2k::ui
