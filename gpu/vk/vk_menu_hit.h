// The Insert menu's layout (vk_menu.cpp draws with these sizes, times its scale) and what a point over it is on: the
// mouse's hover and clicks while the menu is open. Plain arithmetic, no Vulkan or GDI: tests/test_menu_hit.cpp checks it.
// Switch-style card: header, left category sidebar, right option page, footer.
#pragma once

namespace mkwvk::menu_hit {

// ---- the options page, at a scale of 1 (the window 720 pixels high; 1440 and up: 2, and so on) ----
constexpr int kWidth = 900;               // Switch-style card (wider than the old 520 strip)
constexpr int kHeadH = 52;                // title bar
constexpr int kSideW = 180;               // category sidebar
constexpr int kFootH = 40;                // footer hints
constexpr int kContentPad = 14;           // pad under the header into the page
constexpr int kTabY = kHeadH;             // top of the sidebar / body (kept name: tests + menu draw)
constexpr int kTabH = 36;                 // one category row in the sidebar
constexpr int kTabSide = 0;               // unused (was horizontal strip side); L/R live at sidebar bottom
constexpr int kRowGap = kContentPad;      // body pad under the header
constexpr int kRowH = 30;                 // option rows
constexpr int kRowX = 8;                  // inset inside the content pane (from sidebar edge)
constexpr int kValueRight = 20;           // a value ends kValueRight from the right
// slider track in the content pane (absolute from the menu's left)
constexpr int kSliderL = kSideW + 220, kSliderR = kSideW + 380;
constexpr int kSliderGrab = 6;
// ---- the Controls page (bindings) ----
constexpr int kCtlWidth = 900, kCtlTop = 64, kCtlRowH = 24, kCtlPad = 280, kCtlKey = 520, kCtlCellPad = 6;
// ---- Cheats / lists (hit helpers kept for the shared header; MasterSword's menu does not open them) ----
constexpr int kCheatTop = 44, kCheatRowH = 28;
constexpr int kListTop = 48, kListRowH = 24;
constexpr int kListValueRight = 16;

enum Part {
    kNone,      // off the menu's controls (still over the menu: a click does nothing)
    kOutside,   // off the menu
    kTabPrev,   // sidebar L pill
    kTab,       // a category: index
    kTabNext,   // sidebar R pill
    kRow,       // an option's row: index (the tab's own), side -1 the value's left half, 1 its right half, 0 the name
    kSlider,    // a slider row's track: index, permille 0 to 1000 along it
    kCtlRow,    // the Controls page: a row's name (index; index == the count: Reset to defaults)
    kCtlCell,   // a binding: index the row, side 0 the gamepad's, 1 the keyboard's
    kCheatRow,
    kListLine,
};
struct Hit {
    Part part = kNone;
    int index = -1;
    int side = 0;
    int permille = 0;
};

inline void to_image(int x, int y, int win_w, int win_h, int img_w, int img_h, int* ix, int* iy) {
    *ix = win_w > 0 ? (int)((long long)x * img_w / win_w) : x;
    *iy = win_h > 0 ? (int)((long long)y * img_h / win_h) : y;
}
inline void origin(int img_w, int img_h, int menu_w, int menu_h, int* x0, int* y0) {
    *x0 = (img_w - menu_w) / 2;
    *y0 = (img_h - menu_h) / 2;
}
/// Top of the first option row (below header + content pad).
inline int rows_top(int s) { return (kHeadH + kContentPad) * s; }
/// Content pane's left edge (right of the sidebar).
inline int content_x(int s) { return kSideW * s; }
/// Kept for source checks / older call sites: horizontal slot width is unused with a sidebar.
inline int tab_slot_w(int s, int w, int tabs = 4) {
    (void)w;
    (void)tabs;
    return (kTabH + 6) * s;   // vertical step between categories
}
inline int slider_value(int permille, int lo, int hi, int step) {
    if (step <= 0 || hi <= lo) return lo;
    const int steps = (hi - lo) / step;
    const int k = (permille * steps + 500) / 1000;
    return lo + (k < 0 ? 0 : k > steps ? steps : k) * step;
}
inline int slider_permille(int v, int lo, int hi) {
    if (hi <= lo) return 0;
    const int p = (int)((long long)(v - lo) * 1000 / (hi - lo));
    return p < 0 ? 0 : p > 1000 ? 1000 : p;
}

/// Options page hit-test: sidebar categories on the left, rows in the content pane.
inline Hit options(int x, int y, int s, int w, int h, int tabs, int rows, const int* value_l, const bool* slider) {
    Hit hit;
    if (x < 0 || y < 0 || x >= w || y >= h) return hit.part = kOutside, hit;
    if (y < kHeadH * s || y >= h - kFootH * s) return hit;   // title / footer
    const int side = kSideW * s;
    // L / R pills at the bottom of the sidebar
    if (x < side) {
        const int ph = (int)(kTabH * 0.7f) * s;
        const int py = h - kFootH * s - ph - 12 * s;
        if (y >= py && y < py + ph) {
            const int mid = side / 2;
            hit.part = x < mid ? kTabPrev : kTabNext;
            return hit;
        }
        const int iy0 = (kHeadH + 12) * s;
        const int step = (kTabH + 6) * s;
        if (y >= iy0) {
            const int i = (y - iy0) / step;
            if (i >= 0 && i < tabs && y < iy0 + (i + 1) * step - 6 * s) {
                hit.part = kTab;
                hit.index = i;
            }
        }
        return hit;
    }
    const int top = rows_top(s);
    const int cx0 = side + kRowX * s;
    if (y < top || x < cx0 || x >= w - kRowX * s) return hit;
    const int i = (y - top) / (kRowH * s);
    if (i >= rows) return hit;
    hit.index = i;
    if (slider && slider[i] && x >= (kSliderL - kSliderGrab) * s && x < (kSliderR + kSliderGrab) * s) {
        hit.part = kSlider;
        const int p = (int)((long long)(x - kSliderL * s) * 1000 / ((kSliderR - kSliderL) * s));
        hit.permille = p < 0 ? 0 : p > 1000 ? 1000 : p;
        return hit;
    }
    hit.part = kRow;
    const int vl = value_l ? value_l[i] : w;
    const int vr = w - kValueRight * s;
    if (x >= vl && vl < vr) hit.side = x < (vl + vr) / 2 ? -1 : 1;
    return hit;
}
inline int slider_drag(int x, int s) {
    const int p = (int)((long long)(x - kSliderL * s) * 1000 / ((kSliderR - kSliderL) * s));
    return p < 0 ? 0 : p > 1000 ? 1000 : p;
}

inline Hit controls(int x, int y, int s, int w, int h, int count) {
    Hit hit;
    if (x < 0 || y < 0 || x >= w || y >= h) return hit.part = kOutside, hit;
    const int top = kCtlTop * s;
    if (y < top || x < kRowX * s || x >= w - kRowX * s) return hit;
    const int i = (y - top) / (kCtlRowH * s);
    if (i > count) return hit;
    hit.index = i;
    if (i < count && x >= (kCtlPad - kCtlCellPad) * s) {
        hit.part = kCtlCell;
        hit.side = x >= (kCtlKey - kCtlCellPad) * s ? 1 : 0;
    } else {
        hit.part = kCtlRow;
    }
    return hit;
}

inline Hit row_list(int x, int y, int s, int w, int h, int top, int row_h, int count, const int* value_l, Part part) {
    Hit hit;
    if (x < 0 || y < 0 || x >= w || y >= h) return hit.part = kOutside, hit;
    if (y < top * s || x < kRowX * s || x >= w - kRowX * s) return hit;
    const int i = (y - top * s) / (row_h * s);
    if (i >= count) return hit;
    hit.part = part;
    hit.index = i;
    const int vl = value_l ? value_l[i] : w;
    const int vr = w - kListValueRight * s;
    if (x >= vl && vl < vr) hit.side = x < (vl + vr) / 2 ? -1 : 1;
    return hit;
}
inline Hit cheats(int x, int y, int s, int w, int h, int count, const int* value_l) {
    return row_list(x, y, s, w, h, kCheatTop, kCheatRowH, count, value_l, kCheatRow);
}
inline Hit list_lines(int x, int y, int s, int w, int h, int count, const int* value_l) {
    return row_list(x, y, s, w, h, kListTop, kListRowH, count, value_l, kListLine);
}

} // namespace mkwvk::menu_hit
