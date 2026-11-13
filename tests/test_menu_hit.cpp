// The Insert menu's hit-test (gpu/vk/vk_menu_hit.h): Switch-style sidebar layout at scale 1 and 2.
#include "../gpu/vk/vk_menu_hit.h"

#include <cstdio>

#define CHECK(name, condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL %s at line %d\n", name, __LINE__); return 1; } } while (0)

using namespace mkwvk::menu_hit;

int main() {
    int value_l[11];
    bool slider[11] = {};
    for (int& v : value_l) v = 700;
    slider[7] = true;
    const int w1 = kWidth, h1 = 620;
    auto at = [&](int x, int y, int s) {
        int vl[11];
        for (int i = 0; i < 11; i++) vl[i] = value_l[i] * s;
        return options(x, y, s, kWidth * s, h1 * s, 4, 11, vl, slider);
    };
    Hit h;

    // ---- scale 1: sidebar categories (from head+12, step TabH+6) ----
    const int iy0 = (kHeadH + 12);
    h = at(40, iy0 + 8, 1);
    CHECK("tab 0", h.part == kTab && h.index == 0);
    h = at(40, iy0 + (kTabH + 6) + 8, 1);
    CHECK("tab 1", h.part == kTab && h.index == 1);
    h = at(40, iy0 + 2 * (kTabH + 6) + 8, 1);
    CHECK("tab 2", h.part == kTab && h.index == 2);
    h = at(40, iy0 + 3 * (kTabH + 6) + 8, 1);
    CHECK("tab 3", h.part == kTab && h.index == 3);
    // L / R at sidebar bottom
    const int ph = (int)(kTabH * 0.7f), py = h1 - kFootH - ph - 12;
    CHECK("L is previous tab", at(40, py + ph / 2, 1).part == kTabPrev);
    CHECK("R is next tab", at(kSideW - 40, py + ph / 2, 1).part == kTabNext);
    CHECK("the title is no control", at(100, 20, 1).part == kNone);

    // ---- scale 1: the rows ----
    const int top = rows_top(1);
    const int cx = content_x(1) + kRowX + 4;
    h = at(cx, top + 3 * kRowH + 10, 1);
    CHECK("row 3 by its name", h.part == kRow && h.index == 3 && h.side == 0);
    h = at(710, top + 3 * kRowH + 10, 1);
    CHECK("row 3's value, left half", h.part == kRow && h.index == 3 && h.side == -1);
    h = at(820, top + 3 * kRowH + 10, 1);
    CHECK("row 3's value, right half", h.part == kRow && h.index == 3 && h.side == 1);
    h = at(cx, top + 5, 1);
    CHECK("row 0's top line", h.part == kRow && h.index == 0);
    h = at(cx, top - 1, 1);
    CHECK("above the rows", h.part == kNone);
    CHECK("past the last row", at(cx, top + 11 * kRowH + 2, 1).part == kNone);

    // ---- slider track ----
    const int sy = top + 7 * kRowH + kRowH / 2;
    h = at((kSliderL + kSliderR) / 2, sy, 1);
    CHECK("the track's middle", h.part == kSlider && h.index == 7 && h.permille == 500);
    CHECK("a drag past the track holds to its ends", slider_drag(100, 1) == 0 && slider_drag(2000, 1) == 1000);
    CHECK("a drag along it", slider_drag((kSliderL + kSliderR) / 2, 1) == 500);
    CHECK("sharpness at half", slider_value(500, 0, 100, 10) == 50);

    // ---- scale 2 ----
    h = at(80, (iy0 + (kTabH + 6) + 8) * 2, 2);
    CHECK("scale 2: tab 1", h.part == kTab && h.index == 1);
    h = at(cx * 2, (top + 2 * kRowH + 5) * 2, 2);
    CHECK("scale 2: row 2", h.part == kRow && h.index == 2 && h.side == 0);

    CHECK("left of the menu", at(-1, 100, 1).part == kOutside);
    CHECK("right of the menu", at(w1, 100, 1).part == kOutside);

    // ---- Controls page (bindings) ----
    {
        const int cw = kCtlWidth, ch = 720;
        h = controls(50, kCtlTop + 2 * kCtlRowH + 5, 1, cw, ch, 24);
        CHECK("controls: row 2's name", h.part == kCtlRow && h.index == 2);
        h = controls(kCtlPad + 10, kCtlTop + 2 * kCtlRowH + 5, 1, cw, ch, 24);
        CHECK("controls: row 2's gamepad", h.part == kCtlCell && h.index == 2 && h.side == 0);
        h = controls(kCtlKey + 10, kCtlTop + 2 * kCtlRowH + 5, 1, cw, ch, 24);
        CHECK("controls: row 2's keys", h.part == kCtlCell && h.index == 2 && h.side == 1);
        h = controls(50, kCtlTop + 24 * kCtlRowH + 5, 1, cw, ch, 24);
        CHECK("controls: Reset to defaults", h.part == kCtlRow && h.index == 24);
    }

    // ---- cheats / lists helpers still compile ----
    {
        int vl[4] = {420, 420, kWidth, 420};
        h = cheats(30, kCheatTop + 10, 1, kWidth, 400, 4, vl);
        CHECK("cheats: row 0", h.part == kCheatRow && h.index == 0);
        h = list_lines(40, kListTop + 5, 1, kWidth, 400, 4, vl);
        CHECK("list: line 0", h.part == kListLine && h.index == 0);
    }

    std::puts("menu hit-test: all checks passed");
    return 0;
}
