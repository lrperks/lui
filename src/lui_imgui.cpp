// Dear ImGui for lui: setup, theme, and the `ig` Lua module.
//
// lui elements map one-to-one onto ImGui widgets.  lui.lua walks the element
// tree each frame and calls these bindings; ImGui does layout, input, text
// editing and drawing.  Everything here is a thin wrapper over the ImGui call
// of the same name.
#include "imgui.h"
#include "rlImGui.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

// DejaVu Sans in three weights.  Looked up in $LUI_FONT_DIR, then in the
// usual system font folders; ImGui's built-in font is the fallback.
static const char *FONT_FILES[3] = {"DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSans-ExtraLight.ttf"};
static const char *FONT_DIRS[] = {
    "/usr/share/fonts/truetype/dejavu",   // Debian, Ubuntu
    "/usr/share/fonts/dejavu-sans-fonts", // Fedora
    "/usr/share/fonts/TTF",               // Arch
    "/usr/share/fonts/dejavu",
    "/usr/local/share/fonts",
    "/opt/homebrew/share/fonts",          // macOS (brew install font-dejavu)
    "/Library/Fonts",
};
static const float BASE_SIZE = 17.0f;

static ImFont *fonts[3];   // 0 regular, 1 bold, 2 light

// ── helpers ────────────────────────────────────────────────────────────

static ImU32 col32(int rgb, float alpha = 1.0f) {
    return IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, (int)(alpha * 255));
}

static ImVec4 col4(int rgb, float alpha = 1.0f) {
    return ImVec4(((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, alpha);
}

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static ImVec4 shade(ImVec4 c, float k) {
    return ImVec4(clamp01(c.x * k), clamp01(c.y * k), clamp01(c.z * k), c.w);
}

static float num(lua_State *L, int i, float dflt) { return (float)luaL_optnumber(L, i, dflt); }
static bool flag(lua_State *L, int i) { return lua_toboolean(L, i); }

// Font by weight (0 regular, 1 bold, 2 light); anything else is regular.
static ImFont *font_for(int weight) { return fonts[weight >= 0 && weight <= 2 ? weight : 0]; }

// ── setup and theme ────────────────────────────────────────────────────

// Writes the first existing "<dir>/<file>" into out; false if none exists.
static bool find_font(const char *file, char *out, size_t cap) {
    const char *env = getenv("LUI_FONT_DIR");
    for (int i = -1; i < (int)(sizeof FONT_DIRS / sizeof FONT_DIRS[0]); i++) {
        const char *dir = i < 0 ? env : FONT_DIRS[i];
        if (!dir) continue;
        snprintf(out, cap, "%s/%s", dir, file);
        if (FILE *f = fopen(out, "rb")) {
            fclose(f);
            return true;
        }
    }
    return false;
}

static void load_fonts() {
    ImGuiIO &io = ImGui::GetIO();
    char path[1024];
    for (int i = 0; i < 3; i++) {
        fonts[i] = find_font(FONT_FILES[i], path, sizeof path) ? io.Fonts->AddFontFromFileTTF(path, BASE_SIZE) : nullptr;
    }
    if (!fonts[0]) {
        ImFontConfig cfg;
        cfg.SizePixels = BASE_SIZE;
        fonts[0] = io.Fonts->AddFontDefault(&cfg);
    }
    for (ImFont *&f : fonts) if (!f) f = fonts[0];
}

extern "C" void lui_imgui_setup(void) {
    rlImGuiSetLoadFontsCallback(load_fonts);
    rlImGuiBeginInitImGui();
    ImGui::GetIO().IniFilename = nullptr;
    rlImGuiEndInitImGui();
}

extern "C" void lui_imgui_begin(void) { rlImGuiBegin(); }
extern "C" void lui_imgui_end(void) { rlImGuiEnd(); }

// theme(dark, accent, bg): ImGui's own styles, softened — rounder corners,
// roomier spacing, and one accent colour for everything interactive.
static int l_theme(lua_State *L) {
    bool dark = flag(L, 1);
    ImVec4 accent = col4((int)luaL_optinteger(L, 2, 0x3b82f6));
    dark ? ImGui::StyleColorsDark() : ImGui::StyleColorsLight();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 10; s.ChildRounding = 10; s.FrameRounding = 7; s.PopupRounding = 8;
    s.GrabRounding = 7; s.TabRounding = 7; s.ScrollbarRounding = 9;
    s.WindowPadding = ImVec2(18, 16); s.FramePadding = ImVec2(11, 7);
    s.ItemSpacing = ImVec2(10, 9); s.ItemInnerSpacing = ImVec2(8, 6);
    s.WindowBorderSize = 1; s.ChildBorderSize = 1; s.FrameBorderSize = 0;
    s.ScrollbarSize = 12; s.GrabMinSize = 12;

    ImVec4 *c = s.Colors;
    if (dark) {
        c[ImGuiCol_WindowBg] = col4(0x111318);
        c[ImGuiCol_ChildBg] = col4(0x181b22);
        c[ImGuiCol_PopupBg] = col4(0x1b1f27);
        c[ImGuiCol_Border] = col4(0x2a2f3a);
        c[ImGuiCol_FrameBg] = col4(0x222732);
        c[ImGuiCol_FrameBgHovered] = col4(0x2a3040);
        c[ImGuiCol_FrameBgActive] = col4(0x303748);
        c[ImGuiCol_TitleBg] = col4(0x15181e);
        c[ImGuiCol_TitleBgActive] = col4(0x1b1f27);
        c[ImGuiCol_MenuBarBg] = col4(0x15181e);
        c[ImGuiCol_Text] = col4(0xe6e8ee);
        c[ImGuiCol_TextDisabled] = col4(0x7d8597);
    }
    if (lua_isinteger(L, 3)) c[ImGuiCol_WindowBg] = col4((int)lua_tointeger(L, 3));
    c[ImGuiCol_Button] = accent;
    c[ImGuiCol_ButtonHovered] = shade(accent, 1.15f);
    c[ImGuiCol_ButtonActive] = shade(accent, 0.85f);
    c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
    c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.7f);
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = shade(accent, 1.15f);
    c[ImGuiCol_Tab] = ImVec4(accent.x, accent.y, accent.z, 0.2f);
    c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    c[ImGuiCol_TabSelected] = ImVec4(accent.x, accent.y, accent.z, 0.75f);
    c[ImGuiCol_PlotLines] = accent;
    c[ImGuiCol_PlotHistogram] = accent;
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
    c[ImGuiCol_NavCursor] = accent;
    return 0;
}

// ── windows and containers ─────────────────────────────────────────────

// begin_main(): an undecorated window covering the whole screen, for content
// that is not inside a <window>.
static int l_begin_main(lua_State *L) {
    (void)L;
    const ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##lui_main", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_MenuBar * (lua_toboolean(L, 1) ? 1 : 0));
    ImGui::PopStyleVar(2);
    return 0;
}

// begin_window(title, w, h, menubar, x, y) -> visible.  Without x/y a new
// window opens at the top right of the screen.
static int l_begin_window(lua_State *L) {
    const char *title = luaL_checkstring(L, 1);
    float w = num(L, 2, 0), h = num(L, 3, 0);
    if (w > 0 || h > 0) ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_FirstUseEver);
    ImVec2 screen = ImGui::GetMainViewport()->WorkSize;
    float x = num(L, 5, screen.x - (w > 0 ? w : 320) - 24), y = num(L, 6, 56);
    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
    bool visible = ImGui::Begin(title, nullptr, flag(L, 4) ? ImGuiWindowFlags_MenuBar : 0);
    lua_pushboolean(L, visible);
    return 1;
}

static int l_end(lua_State *L) { (void)L; ImGui::End(); return 0; }

// begin_child(id, w, h, border, bg) — h == 0 sizes to the content.
static int l_begin_child(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float w = num(L, 2, 0), h = num(L, 3, 0);
    ImGuiChildFlags flags = flag(L, 4) ? ImGuiChildFlags_Borders : 0;
    if (h == 0) flags |= ImGuiChildFlags_AutoResizeY;
    flags |= ImGuiChildFlags_AlwaysUseWindowPadding;
    bool bg = lua_isinteger(L, 5);
    if (bg) ImGui::PushStyleColor(ImGuiCol_ChildBg, col4((int)lua_tointeger(L, 5)));
    ImGui::BeginChild(id, ImVec2(w, h), flags);
    if (bg) ImGui::PopStyleColor();
    return 0;
}

static int l_end_child(lua_State *L) { (void)L; ImGui::EndChild(); return 0; }
static int l_begin_group(lua_State *L) { (void)L; ImGui::BeginGroup(); return 0; }
static int l_end_group(lua_State *L) { (void)L; ImGui::EndGroup(); return 0; }

// ── text ───────────────────────────────────────────────────────────────

// text(s, wrap)
static int l_text(lua_State *L) {
    const char *s = luaL_checkstring(L, 1);
    if (flag(L, 2)) ImGui::TextWrapped("%s", s);
    else ImGui::TextUnformatted(s);
    return 0;
}

// push_font(weight, size) / pop_font()
static int l_push_font(lua_State *L) {
    ImGui::PushFont(font_for((int)luaL_optinteger(L, 1, 0)), num(L, 2, 0));
    return 0;
}

static int l_pop_font(lua_State *L) { (void)L; ImGui::PopFont(); return 0; }

// push_color(which, rgb): which is "text", "button" (with hover/active shades),
// or "frame".  pop_color(n).
static int l_push_color(lua_State *L) {
    const char *which = luaL_checkstring(L, 1);
    ImVec4 c = col4((int)luaL_checkinteger(L, 2));
    int pushed = 1;
    if (!strcmp(which, "button")) {
        ImGui::PushStyleColor(ImGuiCol_Button, c);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, shade(c, 1.15f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, shade(c, 0.85f));
        pushed = 3;
    } else if (!strcmp(which, "frame")) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, c);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, c);
    }
    lua_pushinteger(L, pushed);
    return 1;
}

static int l_pop_color(lua_State *L) {
    ImGui::PopStyleColor((int)luaL_optinteger(L, 1, 1));
    return 0;
}

// ── widgets ────────────────────────────────────────────────────────────

// button(label, w, h) -> clicked
static int l_button(lua_State *L) {
    lua_pushboolean(L, ImGui::Button(luaL_checkstring(L, 1), ImVec2(num(L, 2, 0), num(L, 3, 0))));
    return 1;
}

// input(label, text, hint, height) -> text, changed, entered.  height > 0 makes it multi-line.
static int l_input(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    size_t len;
    const char *text = luaL_checklstring(L, 2, &len);
    const char *hint = luaL_optstring(L, 3, "");
    float h = num(L, 4, 0);
    std::vector<char> buf(len + 4096);
    memcpy(buf.data(), text, len + 1);
    bool entered;
    if (h > 0) {
        entered = false;
        ImGui::InputTextMultiline(label, buf.data(), buf.size(), ImVec2(ImGui::CalcItemWidth(), h));
    } else {
        entered = ImGui::InputTextWithHint(label, hint, buf.data(), buf.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    }
    lua_pushstring(L, buf.data());
    lua_pushboolean(L, strcmp(buf.data(), text) != 0);
    lua_pushboolean(L, entered);
    return 3;
}

// checkbox(label, checked) -> checked, changed
static int l_checkbox(lua_State *L) {
    bool v = flag(L, 2);
    bool changed = ImGui::Checkbox(luaL_checkstring(L, 1), &v);
    lua_pushboolean(L, v);
    lua_pushboolean(L, changed);
    return 2;
}

// radio(label, active) -> clicked
static int l_radio(lua_State *L) {
    lua_pushboolean(L, ImGui::RadioButton(luaL_checkstring(L, 1), flag(L, 2)));
    return 1;
}

// slider(label, value, min, max, format, integer) -> value, changed
static int l_slider(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    bool changed;
    if (flag(L, 6)) {
        int v = (int)luaL_checknumber(L, 2);
        changed = ImGui::SliderInt(label, &v, (int)num(L, 3, 0), (int)num(L, 4, 100), luaL_optstring(L, 5, "%d"));
        lua_pushinteger(L, v);
    } else {
        float v = (float)luaL_checknumber(L, 2);
        changed = ImGui::SliderFloat(label, &v, num(L, 3, 0), num(L, 4, 1), luaL_optstring(L, 5, "%.2f"));
        lua_pushnumber(L, v);
    }
    lua_pushboolean(L, changed);
    return 2;
}

// drag(label, value, speed, min, max, format) -> value, changed
static int l_drag(lua_State *L) {
    float v = (float)luaL_checknumber(L, 2);
    bool changed = ImGui::DragFloat(luaL_checkstring(L, 1), &v, num(L, 3, 0.1f), num(L, 4, 0), num(L, 5, 0),
                                    luaL_optstring(L, 6, "%.2f"));
    lua_pushnumber(L, v);
    lua_pushboolean(L, changed);
    return 2;
}

// combo(label, index (1-based), items) -> index, changed
static int l_combo(lua_State *L) {
    const char *label = luaL_checkstring(L, 1);
    int index = (int)luaL_optinteger(L, 2, 1) - 1;
    std::vector<const char *> items;
    if (lua_istable(L, 3)) {
        int n = (int)lua_rawlen(L, 3);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, 3, i);
            items.push_back(lua_tostring(L, -1) ? lua_tostring(L, -1) : "");
            lua_pop(L, 1);   // the string stays alive in the table
        }
    }
    bool changed = ImGui::Combo(label, &index, items.data(), (int)items.size());
    lua_pushinteger(L, index + 1);
    lua_pushboolean(L, changed);
    return 2;
}

// color(label, rgb) -> rgb, changed
static int l_color(lua_State *L) {
    int rgb = (int)luaL_optinteger(L, 2, 0);
    float c[3] = {((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f};
    bool changed = ImGui::ColorEdit3(luaL_checkstring(L, 1), c);
    lua_pushinteger(L, ((int)(c[0] * 255 + 0.5f) << 16) | ((int)(c[1] * 255 + 0.5f) << 8) | (int)(c[2] * 255 + 0.5f));
    lua_pushboolean(L, changed);
    return 2;
}

// progress(fraction, w, h, overlay)
static int l_progress(lua_State *L) {
    ImGui::ProgressBar(num(L, 1, 0), ImVec2(num(L, 2, -FLT_MIN), num(L, 3, 0)), luaL_optstring(L, 4, nullptr));
    return 0;
}

// plot(label, values, w, h, histogram)
static int l_plot(lua_State *L) {
    std::vector<float> v;
    if (lua_istable(L, 2)) {
        int n = (int)lua_rawlen(L, 2);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, 2, i);
            v.push_back((float)lua_tonumber(L, -1));
            lua_pop(L, 1);
        }
    }
    ImVec2 size(num(L, 3, 0), num(L, 4, 80));
    if (flag(L, 5)) ImGui::PlotHistogram(luaL_checkstring(L, 1), v.data(), (int)v.size(), 0, nullptr, FLT_MAX, FLT_MAX, size);
    else ImGui::PlotLines(luaL_checkstring(L, 1), v.data(), (int)v.size(), 0, nullptr, FLT_MAX, FLT_MAX, size);
    return 0;
}

// selectable(label, selected) -> clicked
static int l_selectable(lua_State *L) {
    lua_pushboolean(L, ImGui::Selectable(luaL_checkstring(L, 1), flag(L, 2)));
    return 1;
}

// separator(label)
static int l_separator(lua_State *L) {
    if (lua_isstring(L, 1) && *lua_tostring(L, 1)) ImGui::SeparatorText(lua_tostring(L, 1));
    else ImGui::Separator();
    return 0;
}

static int l_same_line(lua_State *L) { ImGui::SameLine(0, num(L, 1, -1)); return 0; }
static int l_spacing(lua_State *L) { ImGui::Dummy(ImVec2(num(L, 1, 0), num(L, 2, 0))); return 0; }
static int l_set_width(lua_State *L) { ImGui::SetNextItemWidth(num(L, 1, -FLT_MIN)); return 0; }

static int l_tooltip(lua_State *L) {
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", luaL_checkstring(L, 1));
    return 0;
}

static int l_hovered(lua_State *L) { lua_pushboolean(L, ImGui::IsItemHovered()); return 1; }

static int l_push_id(lua_State *L) { ImGui::PushID((int)luaL_checkinteger(L, 1)); return 0; }
static int l_pop_id(lua_State *L) { (void)L; ImGui::PopID(); return 0; }

// ── tabs, trees, menus, tables ─────────────────────────────────────────

static int l_begin_tab_bar(lua_State *L) { lua_pushboolean(L, ImGui::BeginTabBar(luaL_checkstring(L, 1))); return 1; }
static int l_end_tab_bar(lua_State *L) { (void)L; ImGui::EndTabBar(); return 0; }
static int l_begin_tab(lua_State *L) { lua_pushboolean(L, ImGui::BeginTabItem(luaL_checkstring(L, 1))); return 1; }
static int l_end_tab(lua_State *L) { (void)L; ImGui::EndTabItem(); return 0; }

// tree(label, open) -> expanded (call tree_pop when expanded)
static int l_tree(lua_State *L) {
    lua_pushboolean(L, ImGui::TreeNodeEx(luaL_checkstring(L, 1), flag(L, 2) ? ImGuiTreeNodeFlags_DefaultOpen : 0));
    return 1;
}

static int l_tree_pop(lua_State *L) { (void)L; ImGui::TreePop(); return 0; }

// collapsing(label, open) -> expanded
static int l_collapsing(lua_State *L) {
    lua_pushboolean(L, ImGui::CollapsingHeader(luaL_checkstring(L, 1), flag(L, 2) ? ImGuiTreeNodeFlags_DefaultOpen : 0));
    return 1;
}

static int l_begin_menu_bar(lua_State *L) { lua_pushboolean(L, ImGui::BeginMenuBar()); return 1; }
static int l_end_menu_bar(lua_State *L) { (void)L; ImGui::EndMenuBar(); return 0; }
static int l_begin_menu(lua_State *L) { lua_pushboolean(L, ImGui::BeginMenu(luaL_checkstring(L, 1))); return 1; }
static int l_end_menu(lua_State *L) { (void)L; ImGui::EndMenu(); return 0; }

// menu_item(label, shortcut, checked) -> clicked
static int l_menu_item(lua_State *L) {
    lua_pushboolean(L, ImGui::MenuItem(luaL_checkstring(L, 1), luaL_optstring(L, 2, nullptr), flag(L, 3)));
    return 1;
}

// begin_table(id, columns, borders) -> visible
static int l_begin_table(lua_State *L) {
    ImGuiTableFlags f = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame;
    if (flag(L, 3)) f |= ImGuiTableFlags_Borders;
    lua_pushboolean(L, ImGui::BeginTable(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2), f));
    return 1;
}

static int l_end_table(lua_State *L) { (void)L; ImGui::EndTable(); return 0; }
static int l_table_column(lua_State *L) { ImGui::TableSetupColumn(luaL_checkstring(L, 1)); return 0; }
static int l_table_headers(lua_State *L) { (void)L; ImGui::TableHeadersRow(); return 0; }
static int l_table_row(lua_State *L) { (void)L; ImGui::TableNextRow(); return 0; }
static int l_table_cell(lua_State *L) { (void)L; ImGui::TableNextColumn(); return 0; }

// ── canvas: free drawing in a reserved rectangle ───────────────────────

// canvas(id, w, h) -> x, y, w, h (screen position and size), hovered, clicked.
// w <= 0 fills the available width.
static int l_canvas(lua_State *L) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float w = num(L, 2, 0), h = num(L, 3, 100);
    if (w <= 0) w = avail.x;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(luaL_checkstring(L, 1), ImVec2(w, h));
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, w);
    lua_pushnumber(L, h);
    lua_pushboolean(L, ImGui::IsItemHovered());
    lua_pushboolean(L, clicked);
    return 6;
}

static ImDrawList *dl() { return ImGui::GetWindowDrawList(); }

// draw_rect(x, y, w, h, radius, rgb, alpha)
static int l_draw_rect(lua_State *L) {
    float x = num(L, 1, 0), y = num(L, 2, 0);
    dl()->AddRectFilled(ImVec2(x, y), ImVec2(x + num(L, 3, 0), y + num(L, 4, 0)), col32((int)luaL_checkinteger(L, 6), num(L, 7, 1)),
                        num(L, 5, 0));
    return 0;
}

// draw_outline(x, y, w, h, radius, rgb, thickness)
static int l_draw_outline(lua_State *L) {
    float x = num(L, 1, 0), y = num(L, 2, 0);
    dl()->AddRect(ImVec2(x, y), ImVec2(x + num(L, 3, 0), y + num(L, 4, 0)), col32((int)luaL_checkinteger(L, 6)), num(L, 5, 0),
                  0, num(L, 7, 1));
    return 0;
}

// draw_gradient(x, y, w, h, top, bottom) — vertical
static int l_draw_gradient(lua_State *L) {
    float x = num(L, 1, 0), y = num(L, 2, 0);
    ImU32 top = col32((int)luaL_checkinteger(L, 5)), bottom = col32((int)luaL_checkinteger(L, 6));
    dl()->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + num(L, 3, 0), y + num(L, 4, 0)), top, top, bottom, bottom);
    return 0;
}

// draw_line(x1, y1, x2, y2, thickness, rgb)
static int l_draw_line(lua_State *L) {
    dl()->AddLine(ImVec2(num(L, 1, 0), num(L, 2, 0)), ImVec2(num(L, 3, 0), num(L, 4, 0)), col32((int)luaL_checkinteger(L, 6)),
                  num(L, 5, 1));
    return 0;
}

// draw_circle(x, y, r, rgb, alpha)
static int l_draw_circle(lua_State *L) {
    dl()->AddCircleFilled(ImVec2(num(L, 1, 0), num(L, 2, 0)), num(L, 3, 1), col32((int)luaL_checkinteger(L, 4), num(L, 5, 1)));
    return 0;
}

// draw_text(s, x, y, size, rgb, weight)
static int l_draw_text(lua_State *L) {
    dl()->AddText(font_for((int)luaL_optinteger(L, 6, 0)), num(L, 4, BASE_SIZE), ImVec2(num(L, 2, 0), num(L, 3, 0)),
                  col32((int)luaL_checkinteger(L, 5)), luaL_checkstring(L, 1));
    return 0;
}

// measure(s, size, weight) -> width
static int l_measure(lua_State *L) {
    ImFont *f = font_for((int)luaL_optinteger(L, 3, 0));
    lua_pushnumber(L, f->CalcTextSizeA(num(L, 2, BASE_SIZE), FLT_MAX, 0, luaL_checkstring(L, 1)).x);
    return 1;
}

// ── registration ───────────────────────────────────────────────────────

static const luaL_Reg ig_fns[] = {
    {"theme", l_theme},
    {"begin_main", l_begin_main}, {"begin_window", l_begin_window}, {"end_window", l_end},
    {"begin_child", l_begin_child}, {"end_child", l_end_child},
    {"begin_group", l_begin_group}, {"end_group", l_end_group},
    {"text", l_text}, {"push_font", l_push_font}, {"pop_font", l_pop_font},
    {"push_color", l_push_color}, {"pop_color", l_pop_color},
    {"button", l_button}, {"input", l_input}, {"checkbox", l_checkbox}, {"radio", l_radio},
    {"slider", l_slider}, {"drag", l_drag}, {"combo", l_combo}, {"color", l_color},
    {"progress", l_progress}, {"plot", l_plot}, {"selectable", l_selectable},
    {"separator", l_separator}, {"same_line", l_same_line}, {"spacing", l_spacing},
    {"set_width", l_set_width}, {"tooltip", l_tooltip}, {"hovered", l_hovered},
    {"push_id", l_push_id}, {"pop_id", l_pop_id},
    {"begin_tab_bar", l_begin_tab_bar}, {"end_tab_bar", l_end_tab_bar},
    {"begin_tab", l_begin_tab}, {"end_tab", l_end_tab},
    {"tree", l_tree}, {"tree_pop", l_tree_pop}, {"collapsing", l_collapsing},
    {"begin_menu_bar", l_begin_menu_bar}, {"end_menu_bar", l_end_menu_bar},
    {"begin_menu", l_begin_menu}, {"end_menu", l_end_menu}, {"menu_item", l_menu_item},
    {"begin_table", l_begin_table}, {"end_table", l_end_table},
    {"table_column", l_table_column}, {"table_headers", l_table_headers},
    {"table_row", l_table_row}, {"table_cell", l_table_cell},
    {"canvas", l_canvas}, {"draw_rect", l_draw_rect}, {"draw_outline", l_draw_outline},
    {"draw_gradient", l_draw_gradient}, {"draw_line", l_draw_line}, {"draw_circle", l_draw_circle},
    {"draw_text", l_draw_text}, {"measure", l_measure},
    {nullptr, nullptr},
};

// Installs the bindings as the global table `ig`.
extern "C" void lui_imgui_lua(lua_State *L) {
    lua_newtable(L);
    luaL_setfuncs(L, ig_fns, 0);
    lua_setglobal(L, "ig");
}
