// MasterSword Insert-menu palettes. Drawn via GDI in vk_menu.cpp.
// 0 OOT Mastersword (default): Lost Woods greens, Master Sword blue, Triforce gold.
// 1 Light: Switch system-settings grey + cyan.
// 2 Dark: near-black panels + red accents.
#pragma once
#include <windows.h>

namespace mkwvk::menu_theme {

enum Id { kOot = 0, kLight = 1, kDark = 2, kCount = 3 };

struct Palette {
    COLORREF backdrop;
    COLORREF panel;
    COLORREF header;
    COLORREF sidebar;
    COLORREF divider;
    COLORREF text;
    COLORREF text_soft;
    COLORREF text_dim;
    COLORREF accent;
    COLORREF accent_text;
    COLORREF row_sel;
    COLORREF row_hover;
    COLORREF help_bg;
    COLORREF warn;
    COLORREF track;
    COLORREF track_fill;
};

inline Palette of(int theme) {
    if (theme == kLight) {
        return Palette{
            RGB(0, 0, 0),
            RGB(240, 240, 242),
            RGB(250, 250, 251),
            RGB(228, 228, 232),
            RGB(206, 206, 212),
            RGB(38, 38, 44),
            RGB(88, 88, 96),
            RGB(138, 138, 146),
            RGB(0, 176, 214),
            RGB(0, 128, 168),
            RGB(255, 255, 255),
            RGB(255, 255, 255),
            RGB(250, 250, 251),
            RGB(206, 92, 20),
            RGB(206, 206, 212),
            RGB(0, 176, 214),
        };
    }
    if (theme == kDark) {
        return Palette{
            RGB(0, 0, 0),
            RGB(24, 24, 28),
            RGB(32, 32, 38),
            RGB(17, 17, 21),
            RGB(56, 56, 64),
            RGB(236, 236, 240),
            RGB(182, 182, 192),
            RGB(128, 128, 140),
            RGB(232, 36, 48),
            RGB(255, 92, 98),
            RGB(44, 44, 52),
            RGB(40, 40, 47),
            RGB(30, 30, 36),
            RGB(255, 156, 64),
            RGB(56, 56, 64),
            RGB(232, 36, 48),
        };
    }
    // OOT Mastersword (default): Kokiri forest, Master Sword hilt blue, logo gold
    return Palette{
        RGB(0, 0, 0),
        RGB(16, 28, 22),       // deep moss panel
        RGB(22, 38, 30),       // header
        RGB(10, 20, 16),       // darker sidebar
        RGB(48, 72, 52),       // moss divider
        RGB(236, 228, 200),    // warm parchment text
        RGB(180, 196, 168),    // soft leaf
        RGB(120, 140, 118),    // dim
        RGB(48, 148, 230),     // Master Sword blue
        RGB(232, 196, 72),     // Triforce / logo gold
        RGB(28, 48, 38),       // selected row
        RGB(24, 40, 32),       // hover
        RGB(14, 26, 20),       // help well
        RGB(220, 140, 48),     // warn (amber)
        RGB(48, 72, 52),       // track
        RGB(48, 148, 230),     // track fill (sword blue)
    };
}

} // namespace mkwvk::menu_theme
