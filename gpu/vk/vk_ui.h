// Overlay drawing primitives for MasterSword's Dear ImGui menus, FPS counter, and start-up screen.
// Adapted from Longshot v3.0.0-rc.3 vk_ui.h, parameterized by menu_theme::Palette so all four themes
// (0 OOT Mastersword, 1 Light, 2 Dark, 3 Longshot) render with full contrast on ImGui draw lists.
#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>

#include "imgui.h"
#include "menu_theme.h"
#include "vk_imgui.h"

namespace mkwvk::i18n {
inline const char* tr(const char* s) { return s; }
inline int plural(int n) { return n == 1 ? 0 : 1; }
} // namespace mkwvk::i18n

namespace mkwvk::ui {

constexpr ImU32 rgba(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }
inline ImU32 alpha(ImU32 c, float k) {
    const int a = (int)((c >> IM_COL32_A_SHIFT & 0xFF) * k);
    return (c & ~IM_COL32_A_MASK) | ((ImU32)std::clamp(a, 0, 255) << IM_COL32_A_SHIFT);
}
inline ImU32 with_alpha(ImU32 c, int a) {
    return (c & ~IM_COL32_A_MASK) | ((ImU32)std::clamp(a, 0, 255) << IM_COL32_A_SHIFT);
}

/// The unit: pixels per point. By the window's height and the Menu size (1 Small, 2 Medium, 3 Large; anything else, Medium)
inline float unit(float h, int size, float* margin) {
    if (size < 1 || size > 3) size = 2;
    const float k = size == 1 ? 0.85f : size == 3 ? 1.3f : 1.0f;
    *margin = h * (size == 3 ? 0.05f : 0.025f);
    return h / 720.0f * k;
}

struct Ctx {
    ImDrawList* d;
    Fonts f;
    float s;      // (pixels per point)
    float w, h;   // (the window)
    float safe;   // (the margin kept clear at each edge, pixels)
    bool pad;     // (a controller was the last input: its buttons in the prompts)
    int confirm_dot = 3, back_dot = 1;
    menu_theme::Palette theme = menu_theme::of(menu_theme::kOot);
};

inline ImVec2 size_of(ImFont* font, float px, const char* t, float wrap = 0) {
    return font->CalcTextSizeA(px, FLT_MAX, wrap, i18n::tr(t));
}

template <class Width>
float fit_px(float px, float min_px, float step, float room, Width width) {
    const float w = width(px);
    if (w <= room) return px;
    if (room <= 0 || w <= 0 || step <= 0) return 0;
    float f = std::floor(px * room / w / step) * step;
    while (f + step < px && width(f + step) <= room) f += step;
    while (f >= min_px && width(f) > room) f -= step;
    return f >= min_px ? f : 0;
}

inline float text(Ctx& c, ImFont* font, float px, ImVec2 p, ImU32 col, const char* t, float wrap = 0) {
    t = i18n::tr(t);
    c.d->AddText(font, px, p, col, t, nullptr, wrap);
    return font->CalcTextSizeA(px, FLT_MAX, wrap, t).x;
}

inline float para(Ctx& c, ImFont* font, float px, ImVec2 p, ImU32 col, const char* t, float wrap, bool draw = true) {
    t = i18n::tr(t);
    const char* end = t + strlen(t);
    float y = 0;
    while (t < end) {
        const char* cut = font->CalcWordWrapPosition(px, t, end, wrap);
        if (cut == t) {
            cut = t + 1;
            while (cut < end && ((unsigned char)*cut & 0xC0) == 0x80) cut++;
        }
        if (draw) c.d->AddText(font, px, ImVec2(p.x, p.y + y), col, t, cut);
        y += px * 1.3f;
        t = cut;
        while (t < end && (*t == ' ' || *t == '\n')) t++;
    }
    return y > 0 ? y - px * 0.3f : 0;
}

inline float mid_top(ImFont* font, float px, float y) { return y - font->GetFontBaked(px)->Ascent + 0.35f * px; }
inline float text_mid(Ctx& c, ImFont* font, float px, float x, float y, ImU32 col, const char* t) {
    return text(c, font, px, ImVec2(x, mid_top(font, px, y)), col, t);
}
inline void text_mid_centre(Ctx& c, ImFont* font, float px, float cx, float y, ImU32 col, const char* t) {
    text_mid(c, font, px, cx - size_of(font, px, t).x * 0.5f, y, col, t);
}
inline void text_mid_right(Ctx& c, ImFont* font, float px, float x, float y, ImU32 col, const char* t) {
    text_mid(c, font, px, x - size_of(font, px, t).x, y, col, t);
}
inline void text_centre(Ctx& c, ImFont* font, float px, ImVec2 p, ImU32 col, const char* t) {
    text(c, font, px, ImVec2(p.x - size_of(font, px, t).x * 0.5f, p.y), col, t);
}

inline void spaced(Ctx& c, ImFont* font, float px, ImVec2 p, ImU32 col, const char* t, float gap) {
    t = i18n::tr(t);
    const auto next = [](const char* q) {
        const char* e = q + 1;
        while (*e && ((unsigned char)*e & 0xC0) == 0x80) e++;
        return e;
    };
    float total = 0;
    for (const char* q = t; *q; q = next(q)) total += font->CalcTextSizeA(px, FLT_MAX, 0, q, next(q)).x + gap;
    float x = p.x - (total - gap) * 0.5f;
    for (const char* q = t; *q; q = next(q)) {
        c.d->AddText(font, px, ImVec2(x, p.y), col, q, next(q));
        x += font->CalcTextSizeA(px, FLT_MAX, 0, q, next(q)).x + gap;
    }
}

inline void shadow(Ctx& c, ImVec2 a, ImVec2 b, float spread, float strength, float dy) {
    const int n = 16;
    for (int i = n; i >= 1; i--) {
        const float k = (float)i / n, e = spread * k;
        c.d->AddRectFilled(a - ImVec2(e, e) + ImVec2(0, dy), b + ImVec2(e, e) + ImVec2(0, dy),
                           rgba(0, 0, 0, (int)(255 * strength * (1 - k) * (1 - k) / 3.5f)), e);
    }
}

inline void triangle(Ctx& c, ImVec2 m, float size, int dir, ImU32 col) {
    const float a = size * 0.5f, b = size * 0.42f;
    switch (dir) {
    case 0: c.d->AddTriangleFilled(m + ImVec2(b, -a), m + ImVec2(-b, 0), m + ImVec2(b, a), col); break;
    case 1: c.d->AddTriangleFilled(m + ImVec2(-b, -a), m + ImVec2(b, 0), m + ImVec2(-b, a), col); break;
    case 2: c.d->AddTriangleFilled(m + ImVec2(-a, b), m + ImVec2(0, -b), m + ImVec2(a, b), col); break;
    default: c.d->AddTriangleFilled(m + ImVec2(-a, -b), m + ImVec2(a, -b), m + ImVec2(0, b), col); break;
    }
}

inline void diamond(Ctx& c, ImVec2 m, float r, ImU32 col) {
    c.d->AddQuadFilled(m + ImVec2(0, -r), m + ImVec2(r, 0), m + ImVec2(0, r), m + ImVec2(-r, 0), col);
}

inline void flourish(Ctx& c, float x0, float x1, float y, ImU32 col) {
    const float s = c.s, m = (x0 + x1) * 0.5f, t = std::max(1.0f, 0.8f * s);
    c.d->AddLine(ImVec2(x0, y), ImVec2(m - 30 * s, y), alpha(col, 0.35f), t);
    c.d->AddLine(ImVec2(m + 30 * s, y), ImVec2(x1, y), alpha(col, 0.35f), t);
    for (int side = -1; side <= 1; side += 2) {
        const float k = (float)side;
        c.d->AddBezierCubic(ImVec2(m + k * 4 * s, y), ImVec2(m + k * 11 * s, y - 7 * s), ImVec2(m + k * 21 * s, y - 6 * s),
                            ImVec2(m + k * 19 * s, y), alpha(col, 0.8f), 1.3f * s);
        c.d->AddBezierCubic(ImVec2(m + k * 19 * s, y), ImVec2(m + k * 17 * s, y + 5 * s), ImVec2(m + k * 11 * s, y + 4 * s),
                            ImVec2(m + k * 12 * s, y + 1 * s), alpha(col, 0.8f), 1.3f * s);
    }
    diamond(c, ImVec2(m, y), 2.2f * s, alpha(col, 0.8f));
}

inline void glow(Ctx& c, ImVec2 a0, ImVec2 b0) {
    const float s = c.s;
    for (int i = 6; i >= 1; i--)
        c.d->AddRect(a0 - ImVec2(i * 1.5f * s, i * 1.5f * s), b0 + ImVec2(i * 1.5f * s, i * 1.5f * s),
                     with_alpha(c.theme.glow_halo, 12 + 4 * (6 - i)), 2 * s, 2.2f * s);
    c.d->AddRect(a0 - ImVec2(1.5f * s, 1.5f * s), b0 + ImVec2(1.5f * s, 1.5f * s), c.theme.glow_col, 2 * s, 3 * s);
}

inline void gold(Ctx& c, ImVec2 a0, ImVec2 b0) {
    c.d->AddRectFilledMultiColor(a0, b0, c.theme.gold_hi, c.theme.gold_hi, c.theme.gold, c.theme.gold);
}

// ---- button prompts ----
enum Glyph { kUpDown, kLeftRight, kTabs, kConfirm, kBack, kKey, kCompact };
struct Prompt {
    Glyph g;
    const char* what;
    const char* key = nullptr;
};

inline float glyph(Ctx& c, float px, ImVec2 p, const Prompt& pr, bool draw) {
    const float s = c.s, hh = px + 10 * s, my = p.y + hh * 0.5f, kpx = px * 0.84f;
    const ImU32 cap = c.theme.keycap, ink = c.theme.keycap_ink, dim = c.theme.keycap_dim;
    auto key = [&](float x, const char* label) {
        const float w = label ? std::max(hh, size_of(c.f.strong, kpx, label).x + 14 * s) : hh;
        if (draw) {
            c.d->AddRectFilled(ImVec2(x, p.y), ImVec2(x + w, p.y + hh), cap, 4 * s);
            if (label) text_mid_centre(c, c.f.strong, kpx, x + w * 0.5f, my, ink, label);
        }
        return w;
    };
    if (pr.g == kKey) return key(p.x, pr.key);
    if (pr.g == kCompact) {
        if (!c.pad) return key(p.x, pr.key ? pr.key : "RShift");
        const float r = hh * 0.5f;
        const ImVec2 m(p.x + r, my);
        if (draw) {
            c.d->AddCircleFilled(m, r, cap);
            const ImVec2 at[4] = {m + ImVec2(0, -r * 0.5f), m + ImVec2(r * 0.5f, 0), m + ImVec2(-r * 0.5f, 0), m + ImVec2(0, r * 0.5f)};
            for (int i = 0; i < 4; i++) {
                if (i == 2) c.d->AddCircleFilled(at[i], r * 0.27f, ink);
                else c.d->AddCircle(at[i], r * 0.22f, dim, 0, std::max(1.0f, s));
            }
        }
        return 2 * r;
    }
    if (!c.pad) {
        switch (pr.g) {
        case kUpDown:
        case kLeftRight: {
            const bool v = pr.g == kUpDown;
            const float w = key(p.x, nullptr);
            key(p.x + w + 3 * s, nullptr);
            if (draw) triangle(c, ImVec2(p.x + w * 0.5f, my), 8 * s, v ? 2 : 0, ink), triangle(c, ImVec2(p.x + w * 1.5f + 3 * s, my), 8 * s, v ? 3 : 1, ink);
            return 2 * w + 3 * s;
        }
        case kTabs: {
            const float w = key(p.x, "Q");
            return w + 3 * s + key(p.x + w + 3 * s, "E");
        }
        case kConfirm: return key(p.x, "Enter");
        default: return key(p.x, "Esc");
        }
    }
    const float r = hh * 0.5f;
    const ImVec2 m(p.x + r, my);
    if (pr.g == kTabs) {
        float x = p.x;
        for (const char* l : {"L", "R"}) {
            const float w = hh * 1.25f;
            if (draw) {
                c.d->AddRectFilled(ImVec2(x, p.y + 2 * s), ImVec2(x + w, p.y + hh - 2 * s), cap, (hh - 4 * s) * 0.5f);
                text_mid_centre(c, c.f.strong, px * 0.8f, x + w * 0.5f, my, ink, l);
            }
            x += w + 3 * s;
        }
        return x - p.x - 3 * s;
    }
    if (!draw) return 2 * r;
    c.d->AddCircleFilled(m, r, cap);
    if (pr.g == kUpDown || pr.g == kLeftRight) {
        const float arm = r * 0.26f, len = r * 0.68f;
        const bool v = pr.g == kUpDown;
        c.d->AddRectFilled(m - ImVec2(arm, len), m + ImVec2(arm, len), v ? ink : dim, arm * 0.5f);
        c.d->AddRectFilled(m - ImVec2(len, arm), m + ImVec2(len, arm), v ? dim : ink, arm * 0.5f);
        c.d->AddRectFilled(m - ImVec2(arm, arm), m + ImVec2(arm, arm), ink);
    } else {
        const int lit = pr.g == kConfirm ? c.confirm_dot : c.back_dot;
        const ImVec2 at[4] = {m + ImVec2(0, -r * 0.5f), m + ImVec2(r * 0.5f, 0), m + ImVec2(-r * 0.5f, 0), m + ImVec2(0, r * 0.5f)};
        for (int i = 0; i < 4; i++) {
            if (i == lit) c.d->AddCircleFilled(at[i], r * 0.27f, ink);
            else c.d->AddCircle(at[i], r * 0.22f, dim, 0, std::max(1.0f, s));
        }
    }
    return 2 * r;
}

inline float prompts(Ctx& c, ImVec2 p, const Prompt* list, int n, bool draw) {
    const float s = c.s, px = 17 * s, hh = px + 10 * s, x0 = p.x;
    for (int i = 0; i < n; i++) {
        if (draw) text_mid(c, c.f.body, px, p.x, p.y + hh * 0.5f, c.theme.ink, list[i].what);
        p.x += size_of(c.f.body, px, list[i].what).x + 8 * s;
        p.x += glyph(c, px, p, list[i], draw) + 26 * s;
    }
    return n ? p.x - x0 - 26 * s : 0;
}

inline float prompt_bar_top(const Ctx& c) { return c.h - c.safe - (17 * c.s + 10 * c.s) - 12 * c.s; }

inline float prompt_bar(Ctx& c, const Prompt* list, int n) {
    const float s = c.s, px = 17 * s, ph = px + 10 * s, py = c.h - c.safe - ph;
    Ctx k = c;
    const float room = c.w - 2 * c.safe - 20 * s, full = prompts(c, ImVec2(0, 0), list, n, false);
    if (full > room) k.s = s * room / full;
    const float pw = prompts(k, ImVec2(0, 0), list, n, false);
    const float x0 = std::max(0.0f, c.w - c.safe - pw - 34 * s);
    if (x0 > 0)
        c.d->AddRectFilledMultiColor(ImVec2(std::max(0.0f, x0 - 80 * s), py - 12 * s), ImVec2(x0, c.h),
                                     alpha(c.theme.bar, 0), c.theme.bar, c.theme.bar, alpha(c.theme.bar, 0));
    c.d->AddRectFilled(ImVec2(x0, py - 12 * s), ImVec2(c.w, c.h), c.theme.bar);
    c.d->AddLine(ImVec2(x0, py - 12 * s), ImVec2(c.w, py - 12 * s), alpha(c.theme.edge, 0.8f), std::max(1.0f, s));
    const float kh = 17 * k.s + 10 * k.s;
    prompts(k, ImVec2(c.w - c.safe - pw - 10 * s, py + (ph - kh) * 0.5f), list, n, true);
    return py - 12 * s;
}

inline void banner(Ctx& c, float hh) {
    const float s = c.s;
    c.d->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(c.w, hh), c.theme.banner_top, c.theme.banner_top, c.theme.banner, c.theme.banner);
    c.d->AddRectFilled(ImVec2(0, hh - 3 * s), ImVec2(c.w, hh), c.theme.banner_edge);
    c.d->AddLine(ImVec2(0, hh - 4 * s), ImVec2(c.w, hh - 4 * s), c.theme.banner_hi, std::max(1.0f, s));
}

inline void box(Ctx& c, ImVec2 a, ImVec2 b) {
    shadow(c, a, b, 18 * c.s, 0.8f, 4 * c.s);
    c.d->AddRectFilled(a, b, c.theme.box);
    c.d->AddRect(a, b, c.theme.box_edge, 0, 1.5f * c.s);
}

inline void button(Ctx& c, ImVec2 a0, ImVec2 b0, const char* label, bool sel) {
    const float s = c.s;
    if (sel) gold(c, a0, b0);
    else c.d->AddRectFilled(a0, b0, c.theme.cell_name);
    c.d->AddRect(a0 + ImVec2(4 * s, 4 * s), b0 - ImVec2(4 * s, 4 * s),
                 sel ? c.theme.inner_frame_sel : c.theme.inner_frame, 0, std::max(1.0f, s));
    c.d->AddRect(a0, b0, c.theme.banner_edge, 0, std::max(1.0f, s));
    ImFont* f = sel ? c.f.strong : c.f.body;
    text_mid_centre(c, f, 20 * s, (a0.x + b0.x) * 0.5f, (a0.y + b0.y) * 0.5f, sel ? c.theme.gold_ink : c.theme.ink, label);
    if (sel) glow(c, a0, b0);
}

} // namespace mkwvk::ui
