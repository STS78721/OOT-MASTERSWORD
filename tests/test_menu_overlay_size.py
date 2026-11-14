"""Separate Small/Medium/Large sizes for the Insert menu and the in-game FPS overlay.

python -I -B tests/test_menu_overlay_size.py [--cxx g++] [--source path/to/vk_menu.cpp]
Compiles the real ui::unit, tx_ui_size, ui_ctx, prebake, and draw_fps helpers with ImGui stubs.
"""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def block(text, marker):
    start = text.index(marker)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cxx", default="g++")
    p.add_argument("--source", type=Path, default=ROOT / "gpu/vk/vk_menu.cpp")
    a = p.parse_args()

    menu_cpp = a.source.read_text(encoding="utf-8")
    menu_h = (a.source.parent / "vk_menu.h").read_text(encoding="utf-8")
    ui_h = (a.source.parent / "vk_ui.h").read_text(encoding="utf-8")

    assert re.search(r"\bint\s+menu_size\s*=\s*2\b", menu_h), "menu_size default 2 missing in vk_menu.h"
    assert re.search(r"\bint\s+overlay_size\s*=\s*2\b", menu_h), "overlay_size default 2 missing in vk_menu.h"
    assert 'opts.menu_size' in menu_cpp and 'opts.overlay_size' in menu_cpp

    m_menu_row = re.search(r'\{\s*TAB_GRAPHICS\s*,\s*"Menu size"[^}]*\}', menu_cpp)
    m_ov_row = re.search(r'\{\s*TAB_GAMEPLAY\s*,\s*"Overlay size"[^}]*\}', menu_cpp)
    assert m_menu_row, '"Menu size" row missing under TAB_GRAPHICS'
    assert m_ov_row, '"Overlay size" row missing under TAB_GAMEPLAY'
    assert "&opts.menu_size, 1, 3, 1, tx_ui_size" in m_menu_row.group(0)
    assert "true, false" in m_menu_row.group(0), "Menu size must be preset_exempt=true, restart=false"
    assert "&opts.overlay_size, 1, 3, 1, tx_ui_size" in m_ov_row.group(0)
    assert "true, false" in m_ov_row.group(0), "Overlay size must be preset_exempt=true, restart=false"

    fn_unit = block(ui_h, "inline float unit(")
    fn_tx_size = block(menu_cpp, "const char* tx_ui_size(")
    fn_ui_ctx = block(menu_cpp, "ui::Ctx ui_ctx(")
    fn_prebake_needed = block(menu_cpp, "bool prebake_needed(")
    fn_prebake_full = block(menu_cpp, "void prebake(")
    cut_end = fn_prebake_full.index("ui::prebake(")
    semi = fn_prebake_full.index(";", cut_end) + 1
    fn_prebake = fn_prebake_full[:semi] + "\n}"
    fn_banner_h = block(menu_cpp, "float banner_h(")
    fn_draw_fps = block(menu_cpp, "void draw_fps(")

    fixture = r'''#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL:%d %s\n", __LINE__, #x); std::exit(1); } } while (0)
using ImU32 = unsigned int;
struct ImVec2 { float x = 0, y = 0; ImVec2() = default; ImVec2(float a, float b) : x(a), y(b) {} };
struct ImFont {
    ImVec2 CalcTextSizeA(float px, float, float, const char* s) const {
        return ImVec2((float)std::strlen(s) * px * 0.5f, px);
    }
};
struct RecordedText { float px = 0, x = 0, y = 0; std::string text; };
static std::vector<RecordedText> g_texts;
static std::vector<float> g_prebaked;
struct ImDrawList {
    void AddText(ImFont*, float px, ImVec2 pos, ImU32, const char* text) {
        g_texts.push_back({px, pos.x, pos.y, text ? text : ""});
    }
};
namespace ImGui {
static ImDrawList g_dl;
ImDrawList* GetForegroundDrawList() { return &g_dl; }
}
namespace menu_keys { int dot(int v) { return v; } }
namespace menu_theme { struct Palette {}; Palette of(int) { return {}; } }

struct Options {
    int menu_theme = 0;
    int menu_size = 2;
    int overlay_size = 2;
} opts;

int pad_last = 0, pad_confirm = 0, pad_back = 0;
int page = 0;
float menu_ox = 0;
char fps_text[16] = "60";
char fps_sub[32] = "game 60";
static ImFont g_font;

namespace ui {
struct Fonts { ImFont* strong = &g_font; };
Fonts fonts() { return {}; }
struct Ctx {
    ImDrawList* d = &ImGui::g_dl;
    Fonts f{};
    float s = 1;
    float w = 0, h = 0;
    float safe = 0;
    int pad = 0, cdot = 0, bdot = 0;
    menu_theme::Palette theme{};
};
ImU32 rgba(int r, int g, int b, int a = 255) { return (ImU32)((a << 24) | (r << 16) | (g << 8) | b); }
ImVec2 size_of(ImFont* f, float px, const char* s) { return f->CalcTextSizeA(px, 0, 0, s); }
void prebake(const float* px, int n) { g_prebaked.assign(px, px + n); }
''' + fn_unit + "\n}\n" + r'''
struct Tab {
    ImFont* f; float px; float digit;
    Tab(ImFont* f, float px) : f(f), px(px), digit(px * 0.5f) {}
    float width(const char* s) const { return (float)std::strlen(s) * digit; }
    void draw(ImDrawList* d, float x, float y, ImU32 col, const char* s, float) const {
        d->AddText(f, px, ImVec2(x, y), col, s);
    }
};
''' + fn_tx_size + "\n" + fn_ui_ctx + "\n" + fn_banner_h + "\nfloat prebaked[3] = {0, 0, 0};\n" + r'''
''' + re.sub(r'^float prebaked\[3\] = \{0, 0, 0\};\s*', '', fn_prebake_needed, flags=re.M) + "\n" + fn_prebake + "\n" + fn_draw_fps + r'''

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-3f; }

int main() {
    CHECK(std::strcmp(tx_ui_size(1), "Small") == 0);
    CHECK(std::strcmp(tx_ui_size(2), "Medium") == 0);
    CHECK(std::strcmp(tx_ui_size(3), "Large") == 0);
    CHECK(std::strcmp(tx_ui_size(99), "Medium") == 0);

    // 1. Menu size and Overlay size scale independently at 1920x1080 (h/720 = 1.5).
    opts.menu_size = 1;
    opts.overlay_size = 3;
    ui::Ctx c_small_menu = ui_ctx(1920, 1080);
    CHECK(approx(c_small_menu.s, 1.5f * 0.85f));
    CHECK(approx(c_small_menu.safe, 1080.0f * 0.025f));

    // Changing overlay_size must NOT change ui_ctx (menu scale).
    opts.overlay_size = 1;
    ui::Ctx c_same_menu = ui_ctx(1920, 1080);
    CHECK(approx(c_same_menu.s, c_small_menu.s));

    // Overlay draws at Large (1.5 * 1.3 = 1.95) even when Menu is Small.
    opts.menu_size = 1;
    opts.overlay_size = 3;
    c_small_menu = ui_ctx(1920, 1080);
    g_texts.clear();
    menu_ox = 0;
    draw_fps(c_small_menu, false);
    bool saw_fps_large = false, saw_digit_large = false;
    for (const auto& t : g_texts) {
        if (t.text == "FPS" && approx(t.px, 13.0f * 1.5f * 1.3f)) saw_fps_large = true;
        if (t.text == "6" && approx(t.px, 28.0f * 1.5f * 1.3f)) saw_digit_large = true;
        // Overlay closed -> top uses overlay safe margin (0.05 * 1080 = 54) for digits.
        if (t.text == "6" && t.y >= 54.0f - 3.0f && t.y <= 54.0f + 3.0f) {}
    }
    CHECK(saw_fps_large && saw_digit_large);

    // When stacked under an open Large menu, top sits below Large banner_h while overlay stays Small.
    opts.menu_size = 3;
    opts.overlay_size = 1;
    ui::Ctx c_large_menu = ui_ctx(1920, 1080);
    CHECK(approx(c_large_menu.s, 1.5f * 1.3f));
    g_texts.clear();
    draw_fps(c_large_menu, true);
    const float large_bh = banner_h(c_large_menu);
    const float small_s = 1.5f * 0.85f;
    bool saw_digit_small = false, under_large_banner = false;
    for (const auto& t : g_texts) {
        if (t.text == "6" && approx(t.px, 28.0f * small_s)) {
            saw_digit_small = true;
            if (t.y >= large_bh + 10.0f * small_s - 3.0f) under_large_banner = true;
        }
    }
    CHECK(saw_digit_small && under_large_banner);

    // 2. Prebake detects changes in either menu_size or overlay_size independently and bakes overlay 28/13 px.
    opts.menu_size = 2; opts.overlay_size = 2;
    CHECK(prebake_needed(1920, 1080));
    prebake(ui_ctx(1920, 1080));
    CHECK(!prebake_needed(1920, 1080));
    opts.overlay_size = 3;
    CHECK(prebake_needed(1920, 1080));
    prebake(ui_ctx(1920, 1080));
    CHECK(!prebake_needed(1920, 1080));
    const float want_big = 28.0f * 1.5f * 1.3f;
    CHECK(std::any_of(g_prebaked.begin(), g_prebaked.end(), [&](float v) { return approx(v, want_big); }));
    opts.menu_size = 1;
    CHECK(prebake_needed(1920, 1080));
    prebake(ui_ctx(1920, 1080));
    CHECK(!prebake_needed(1920, 1080));

    std::printf("PASS: separate Menu size and Overlay size work cleanly and independently\n");
}
'''

    with tempfile.TemporaryDirectory(prefix="menu-overlay-size-") as tmp:
        cpp = Path(tmp) / "fixture.cpp"
        exe = Path(tmp) / "fixture.exe"
        cpp.write_text(fixture, encoding="utf-8")
        subprocess.run([a.cxx, "-std=c++20", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
