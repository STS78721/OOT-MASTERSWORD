// MasterSword Insert-menu palettes for Dear ImGui overlays (vk_ui.h / vk_menu.cpp).
// 0 OOT Mastersword (default): Kokiri forest greens, Master Sword blue, Triforce gold.
// 1 Light: Switch system-settings bright white/grey + crisp cyan-blue.
// 2 Dark: near-black panels + crimson red accents.
// 3 Longshot: Longshot RC3 warm parchment, bronze cells, and gold.
// 4 OOT Options (Amber): native OoT Options warm mahogany leather, copper banner, amber-gold pills, white focus frame.
#pragma once
#include <algorithm>
#include <cstdint>

namespace mkwvk::menu_theme {

enum Id { kOot = 0, kLight = 1, kDark = 2, kLongshot = 3, kOotAmber = 4, kCount = 5 };

// Matches IM_COL32(r, g, b, a) layout without dragging imgui.h into standalone tests.
constexpr uint32_t col32(int r, int g, int b, int a = 255) {
    return ((uint32_t)(a & 0xFF) << 24) | ((uint32_t)(b & 0xFF) << 16) | ((uint32_t)(g & 0xFF) << 8) | (uint32_t)(r & 0xFF);
}

struct Palette {
    uint32_t dim_bg;          // screen dim behind the menu
    uint32_t ink;             // primary readable body text
    uint32_t ink_bright;      // highlighted row/active title text
    uint32_t soft;            // secondary help / caption text
    uint32_t dim;             // subtle labels & inactive option text
    uint32_t gold;            // selected cell / progress bar bottom fill
    uint32_t gold_hi;         // selected cell / progress bar top fill
    uint32_t gold_ink;        // text & glyphs drawn on top of gold fill
    uint32_t accent;          // scrollbar, help rule, slider fill (unselected)
    uint32_t heading_col;     // section headers & restart diamonds/text
    uint32_t edge;            // cell & column dividers / outlines
    uint32_t cell_dark;       // value cell background
    uint32_t cell_name;       // unselected row name cell background
    uint32_t cell_name_sel;   // selected row name cell background
    uint32_t bronze;          // unselected toggle-half highlight pill
    uint32_t column;          // main option column backdrop
    uint32_t banner_top;      // top banner gradient top
    uint32_t banner;          // top banner gradient bottom
    uint32_t banner_ink;      // inactive tabs, version & banner title text
    uint32_t banner_edge;     // banner bottom border & button outer edge
    uint32_t banner_hi;       // subtle upper hairline above banner edge
    uint32_t tab_inset;       // active tab fill
    uint32_t tab_inset_edge;  // active tab vertical side borders
    uint32_t tab_ink;         // active tab & banner keycap label text
    uint32_t box;             // help box & restart dialog background
    uint32_t box_edge;        // help box & dialog outline
    uint32_t bar;             // bottom button-prompt bar background
    uint32_t listen;          // waiting-for-binding fill
    uint32_t keycap;          // keycap & controller icon fill in prompts
    uint32_t keycap_ink;      // key letter / lit controller dot
    uint32_t keycap_dim;      // unlit controller dots
    uint32_t glow_col;        // crisp inner focus frame around active value cell
    uint32_t glow_halo;       // soft outer focus glow RGB (alpha added per ring)
    uint32_t inner_frame_sel; // subtle inner button border when selected
    uint32_t inner_frame;     // subtle inner button border when unselected
    uint32_t slider_track;    // slider track when unselected
    uint32_t slider_track_sel;// slider track when selected
    uint32_t scroll_track;    // scrollbar backing track
    uint32_t startup_bg;      // start-up prewarm backdrop
    uint32_t startup_glow;    // subtle radial glow RGB under logo
    uint32_t startup_title;   // spaced brand wordmark
};

inline Palette of(int theme) {
    switch (std::clamp(theme, 0, (int)kCount - 1)) {
    case kLight:
        return Palette{
            col32(16, 20, 28, 175),    // dim_bg
            col32(28, 32, 38),         // ink
            col32(12, 16, 24),         // ink_bright
            col32(64, 70, 82),         // soft
            col32(100, 106, 118),      // dim
            col32(0, 122, 166),        // gold (Switch cyan-blue bottom)
            col32(0, 144, 192),        // gold_hi (Switch cyan-blue top)
            col32(255, 255, 255),      // gold_ink (crisp white on cyan)
            col32(0, 138, 182),        // accent
            col32(176, 82, 0),         // heading_col (high-contrast amber/rust)
            col32(178, 184, 196, 210), // edge
            col32(224, 227, 234, 248), // cell_dark
            col32(232, 234, 240, 244), // cell_name
            col32(255, 255, 255, 252), // cell_name_sel
            col32(198, 204, 214),      // bronze (unselected toggle fill)
            col32(242, 243, 247, 240), // column
            col32(248, 249, 252, 248), // banner_top
            col32(236, 238, 244, 248), // banner
            col32(34, 38, 46),         // banner_ink
            col32(182, 188, 198),      // banner_edge
            col32(255, 255, 255, 200), // banner_hi
            col32(0, 122, 166, 250),   // tab_inset
            col32(0, 96, 134),         // tab_inset_edge
            col32(255, 255, 255),      // tab_ink
            col32(249, 250, 252, 250), // box
            col32(172, 178, 190, 235), // box_edge
            col32(235, 237, 242, 244), // bar
            col32(216, 152, 40),       // listen
            col32(42, 48, 58),         // keycap
            col32(250, 252, 255),      // keycap_ink
            col32(160, 168, 180),      // keycap_dim
            col32(0, 118, 162),        // glow_col
            col32(0, 148, 196, 0),     // glow_halo
            col32(255, 255, 255, 90),  // inner_frame_sel
            col32(120, 128, 144, 90),  // inner_frame
            col32(0, 0, 0, 48),        // slider_track
            col32(255, 255, 255, 100), // slider_track_sel
            col32(0, 0, 0, 42),        // scroll_track
            col32(238, 241, 246),      // startup_bg
            col32(0, 160, 206, 4),     // startup_glow
            col32(24, 28, 36),         // startup_title
        };
    case kDark:
        return Palette{
            col32(4, 4, 6, 185),       // dim_bg
            col32(236, 236, 242),      // ink
            col32(255, 255, 255),      // ink_bright
            col32(182, 184, 194),      // soft
            col32(142, 144, 156),      // dim
            col32(198, 30, 42),        // gold (crimson fill bottom)
            col32(224, 42, 54),        // gold_hi (crimson fill top)
            col32(255, 248, 248),      // gold_ink (crisp white on crimson)
            col32(232, 48, 60),        // accent
            col32(255, 110, 116),      // heading_col
            col32(72, 72, 84, 175),    // edge
            col32(20, 20, 26, 248),    // cell_dark
            col32(34, 34, 42, 242),    // cell_name
            col32(56, 46, 52, 248),    // cell_name_sel
            col32(52, 52, 62),         // bronze
            col32(20, 20, 25, 234),    // column
            col32(38, 38, 46, 248),    // banner_top
            col32(28, 28, 34, 248),    // banner
            col32(226, 226, 236),      // banner_ink
            col32(68, 68, 80),         // banner_edge
            col32(118, 118, 136, 95),  // banner_hi
            col32(192, 28, 40, 250),   // tab_inset
            col32(120, 20, 28),        // tab_inset_edge
            col32(255, 250, 250),      // tab_ink
            col32(24, 24, 30, 250),    // box
            col32(84, 84, 98, 220),    // box_edge
            col32(14, 14, 18, 236),    // bar
            col32(178, 92, 32),        // listen
            col32(236, 236, 242),      // keycap
            col32(24, 24, 30),         // keycap_ink
            col32(140, 140, 152),      // keycap_dim
            col32(255, 132, 138),      // glow_col
            col32(232, 48, 60, 0),     // glow_halo
            col32(255, 255, 255, 75),  // inner_frame_sel
            col32(120, 120, 138, 90),  // inner_frame
            col32(255, 255, 255, 40),  // slider_track
            col32(255, 255, 255, 95),  // slider_track_sel
            col32(0, 0, 0, 150),       // scroll_track
            col32(12, 12, 15),         // startup_bg
            col32(84, 18, 24, 4),      // startup_glow
            col32(244, 244, 250),      // startup_title
        };
    case kLongshot:
        return Palette{
            col32(8, 5, 2, 185),       // dim_bg
            col32(240, 232, 214),      // ink
            col32(255, 250, 240),      // ink_bright
            col32(214, 204, 184),      // soft
            col32(168, 156, 136),      // dim
            col32(222, 152, 40),       // gold
            col32(238, 168, 52),       // gold_hi
            col32(36, 24, 8),          // gold_ink
            col32(214, 170, 96),       // accent
            col32(238, 168, 52),       // heading_col
            col32(122, 98, 64, 150),   // edge
            col32(30, 25, 19, 246),    // cell_dark
            col32(60, 47, 32, 240),    // cell_name
            col32(92, 70, 42, 248),    // cell_name_sel
            col32(84, 64, 38),         // bronze
            col32(34, 26, 17, 228),    // column
            col32(184, 146, 96, 246),  // banner_top
            col32(176, 138, 88, 246),  // banner
            col32(46, 30, 12),         // banner_ink
            col32(70, 48, 24),         // banner_edge
            col32(226, 192, 138, 140), // banner_hi
            col32(84, 56, 30, 250),    // tab_inset
            col32(52, 34, 16),         // tab_inset_edge
            col32(255, 250, 240),      // tab_ink
            col32(36, 28, 19, 248),    // box
            col32(122, 98, 64, 220),   // box_edge
            col32(22, 17, 11, 232),    // bar
            col32(150, 120, 40),       // listen
            col32(240, 232, 214),      // keycap
            col32(40, 30, 20),         // keycap_ink
            col32(150, 140, 126),      // keycap_dim
            col32(255, 250, 240),      // glow_col
            col32(255, 244, 220, 0),   // glow_halo
            col32(36, 24, 8, 60),      // inner_frame_sel
            col32(190, 154, 98, 90),   // inner_frame
            col32(255, 255, 255, 40),  // slider_track
            col32(36, 24, 8, 80),      // slider_track_sel
            col32(0, 0, 0, 140),       // scroll_track
            col32(14, 10, 6),          // startup_bg
            col32(46, 33, 18, 4),      // startup_glow
            col32(246, 232, 204),      // startup_title
        };
    case kOotAmber:
        // Native OoT Options palette: warm mahogany leather, terracotta-copper header, vibrant amber-gold pills, crisp white focus
        return Palette{
            col32(18, 10, 9, 178),     // dim_bg
            col32(248, 240, 230),      // ink (crisp warm ivory)
            col32(255, 252, 246),      // ink_bright
            col32(224, 206, 192),      // soft
            col32(184, 162, 148),      // dim
            col32(214, 128, 16),       // gold (rich golden-amber button bottom)
            col32(238, 154, 28),       // gold_hi (vibrant amber button top)
            col32(255, 253, 248),      // gold_ink (crisp white on amber, native Options style)
            col32(240, 168, 56),       // accent (warm amber scrollbar & sliders)
            col32(248, 192, 92),       // heading_col (warm golden-tan headings)
            col32(166, 122, 94, 185),  // edge (parchment-bronze dividers)
            col32(72, 58, 62, 246),    // cell_dark (smoky plum-espresso inactive toggle well)
            col32(138, 95, 76, 242),   // cell_name (warm dusty-terracotta row label strip)
            col32(166, 118, 94, 248),  // cell_name_sel (brightened terracotta when selected)
            col32(100, 82, 84),        // bronze (unselected toggle-half highlight)
            col32(66, 42, 39, 238),    // column (deep mahogany leather column)
            col32(202, 128, 82, 248),  // banner_top (warm copper header top)
            col32(150, 86, 52, 248),   // banner (terracotta header bottom)
            col32(252, 244, 234),      // banner_ink (warm white header title & inactive tabs)
            col32(84, 48, 32),         // banner_edge
            col32(232, 174, 122, 165), // banner_hi (delicate warm-gold header trim)
            col32(220, 136, 20, 250),  // tab_inset (vibrant amber active tab)
            col32(255, 224, 152),      // tab_inset_edge
            col32(255, 252, 246),      // tab_ink
            col32(78, 50, 41, 244),    // box (warm leather description panel)
            col32(178, 138, 106, 225), // box_edge (soft parchment-tan outline)
            col32(34, 22, 20, 240),    // bar (deep dark espresso prompt bar)
            col32(198, 126, 26),       // listen
            col32(248, 242, 234),      // keycap
            col32(46, 28, 22),         // keycap_ink
            col32(158, 140, 130),      // keycap_dim
            col32(255, 250, 242),      // glow_col (luminous white focus frame around active value)
            col32(255, 236, 204, 0),   // glow_halo
            col32(255, 252, 244, 115), // inner_frame_sel
            col32(208, 164, 126, 105), // inner_frame
            col32(255, 255, 255, 48),  // slider_track
            col32(255, 255, 255, 110), // slider_track_sel
            col32(32, 18, 16, 155),    // scroll_track
            col32(34, 21, 18),         // startup_bg
            col32(174, 102, 46, 5),    // startup_glow
            col32(252, 238, 216),      // startup_title
        };
    case kOot:
    default:
        // Kokiri forest greens, Master Sword hilt blue, Triforce gold
        return Palette{
            col32(4, 10, 7, 185),      // dim_bg
            col32(236, 228, 200),      // ink (warm parchment)
            col32(255, 250, 232),      // ink_bright
            col32(184, 202, 172),      // soft (soft leaf)
            col32(138, 160, 134),      // dim
            col32(216, 172, 44),       // gold (Triforce gold bottom)
            col32(238, 198, 68),       // gold_hi (Triforce gold top)
            col32(24, 22, 10),         // gold_ink
            col32(64, 164, 240),       // accent (Master Sword hilt blue)
            col32(238, 198, 74),       // heading_col (Triforce gold)
            col32(58, 92, 66, 175),    // edge (moss divider)
            col32(14, 25, 19, 248),    // cell_dark
            col32(24, 44, 34, 242),    // cell_name
            col32(36, 68, 52, 248),    // cell_name_sel
            col32(36, 62, 46),         // bronze (unselected toggle fill)
            col32(15, 27, 21, 232),    // column (deep moss panel)
            col32(26, 50, 38, 248),    // banner_top
            col32(20, 40, 30, 248),    // banner
            col32(236, 228, 200),      // banner_ink
            col32(52, 84, 60),         // banner_edge
            col32(96, 148, 110, 115),  // banner_hi
            col32(34, 96, 152, 248),   // tab_inset (Master Sword blue)
            col32(72, 168, 240),       // tab_inset_edge
            col32(255, 250, 236),      // tab_ink
            col32(16, 28, 22, 248),    // box
            col32(64, 100, 74, 225),   // box_edge
            col32(10, 19, 15, 236),    // bar
            col32(158, 126, 36),       // listen
            col32(236, 228, 200),      // keycap
            col32(20, 32, 24),         // keycap_ink
            col32(124, 144, 122),      // keycap_dim
            col32(248, 220, 120),      // glow_col
            col32(72, 168, 240, 0),    // glow_halo (subtle sword-blue halo)
            col32(24, 22, 10, 65),     // inner_frame_sel
            col32(96, 156, 212, 95),   // inner_frame
            col32(255, 255, 255, 42),  // slider_track
            col32(24, 22, 10, 85),     // slider_track_sel
            col32(0, 0, 0, 150),       // scroll_track
            col32(10, 18, 14),         // startup_bg
            col32(30, 76, 54, 4),      // startup_glow
            col32(246, 238, 208),      // startup_title
        };
    }
}

} // namespace mkwvk::menu_theme
