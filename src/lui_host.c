/* lui_host — runs a luic-compiled chunk on a raylib window with Lua 5.3.
 *
 *   ./luic counter.lui
 *   ./lui_host counter.lui.lua
 *
 * Exposes a `host` table to lui.lua and loads lui.lua as module "lui". */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* Dear ImGui (lui_imgui.cpp) on top of rlImGui. */
void lui_imgui_setup(void);
void lui_imgui_begin(void);
void lui_imgui_end(void);
int lui_imgui_field(const char *id, float x, float y, float w, float h,
                    char *buf, int cap, const char *hint, float fontSize);

#define FONT_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
#define FONT_BOLD "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

#define MIN_W 240
#define MIN_H 160

static Font regular, bold;
static int have_fonts = 0;

static Color rgb(lua_State *L, int idx, int alpha) {
    int c = (int)luaL_checkinteger(L, idx);
    return (Color){(c >> 16) & 255, (c >> 8) & 255, c & 255, (unsigned char)alpha};
}

static Font pick(int weight) { return (weight == 1 && have_fonts) ? bold : regular; }

static int h_open(lua_State *L) {
    const char *title = luaL_checkstring(L, 1);
    /* open(title, width, height, resizable = true) */
    int resizable = lua_isnoneornil(L, 4) || lua_toboolean(L, 4);
    SetConfigFlags(FLAG_MSAA_4X_HINT | (resizable ? FLAG_WINDOW_RESIZABLE : 0));
    InitWindow((int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), title);
    SetWindowMinSize(MIN_W, MIN_H);
    lui_imgui_setup();
    SetTargetFPS(60);
    if (FileExists(FONT_PATH)) {
        regular = LoadFontEx(FONT_PATH, 48, NULL, 0);
        bold = FileExists(FONT_BOLD) ? LoadFontEx(FONT_BOLD, 48, NULL, 0) : regular;
        SetTextureFilter(regular.texture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(bold.texture, TEXTURE_FILTER_BILINEAR);
        have_fonts = 1;
    }
    return 0;
}

/* textfield(id, x, y, w, h, text, placeholder, size)
 *   -> newText, entered, changed */
static int h_textfield(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3);
    float w = (float)luaL_checknumber(L, 4), h = (float)luaL_checknumber(L, 5);
    const char *text = luaL_checkstring(L, 6);
    const char *hint = luaL_optstring(L, 7, "");
    float size = (float)luaL_optnumber(L, 8, 18.0);

    char buf[4096];
    snprintf(buf, sizeof buf, "%s", text);
    int flags = lui_imgui_field(id, x, y, w, h, buf, sizeof buf, hint, size);
    lua_pushstring(L, buf);
    lua_pushboolean(L, flags & 1);
    lua_pushboolean(L, flags & 2);
    return 3;
}

/* size() -> current window width, height (changes when the user resizes) */
static int h_size(lua_State *L) {
    lua_pushinteger(L, GetScreenWidth());
    lua_pushinteger(L, GetScreenHeight());
    return 2;
}

static int h_should_close(lua_State *L) {
    lua_pushboolean(L, WindowShouldClose());
    return 1;
}

static int h_begin(lua_State *L) {
    BeginDrawing();
    ClearBackground(rgb(L, 1, 255));
    lui_imgui_begin();
    return 0;
}

static int h_finish(lua_State *L) {
    (void)L;
    lui_imgui_end();
    EndDrawing();
    return 0;
}

/* rect(x, y, w, h, radius, color) */
static int h_rect(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1), y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3), h = (float)luaL_checknumber(L, 4);
    float radius = (float)luaL_checknumber(L, 5);
    Color c = rgb(L, 6, 255);
    float m = w < h ? w : h;
    if (m <= 0) return 0;
    float rn = radius * 2.0f / m;
    if (rn > 1.0f) rn = 1.0f;
    if (rn <= 0.0f) DrawRectangleV((Vector2){x, y}, (Vector2){w, h}, c);
    else DrawRectangleRounded((Rectangle){x, y, w, h}, rn, 6, c);
    return 0;
}

/* text(s, x, y, size, weight, color) */
static int h_text(lua_State *L) {
    const char *s = luaL_checkstring(L, 1);
    Vector2 p = {(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)};
    float size = (float)luaL_checknumber(L, 4);
    int weight = (int)luaL_optinteger(L, 5, 0);
    DrawTextEx(pick(weight), s, p, size, 0.0f, rgb(L, 6, 255));
    return 0;
}

/* measure(s, size, weight) -> width */
static int h_measure(lua_State *L) {
    const char *s = luaL_checkstring(L, 1);
    float size = (float)luaL_checknumber(L, 2);
    int weight = (int)luaL_optinteger(L, 3, 0);
    lua_pushinteger(L, (lua_Integer)MeasureTextEx(pick(weight), s, size, 0.0f).x);
    return 1;
}

/* mouse() -> x, y, down, pressed, released */
static int h_mouse(lua_State *L) {
    lua_pushinteger(L, GetMouseX());
    lua_pushinteger(L, GetMouseY());
    lua_pushboolean(L, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    lua_pushboolean(L, IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
    lua_pushboolean(L, IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    return 5;
}

/* outline(x, y, w, h, radius, color, thickness) */
static int h_outline(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1), y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3), h = (float)luaL_checknumber(L, 4);
    float radius = (float)luaL_checknumber(L, 5);
    Color c = rgb(L, 6, 255);
    float thick = (float)luaL_optnumber(L, 7, 1.0);
    float m = w < h ? w : h;
    if (m <= 0) return 0;
    float rn = radius * 2.0f / m;
    if (rn > 1.0f) rn = 1.0f;
    DrawRectangleRoundedLinesEx((Rectangle){x, y, w, h}, rn, 6, thick, c);
    return 0;
}

/* Appends codepoint cp to buf as UTF-8; returns the new length. */
static int put_utf8(char *buf, int n, int cp) {
    if (cp < 0x80) {
        buf[n++] = (char)cp;
    } else if (cp < 0x800) {
        buf[n++] = (char)(0xC0 | (cp >> 6));
        buf[n++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        buf[n++] = (char)(0xE0 | (cp >> 12));
        buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (cp & 0x3F));
    } else {
        buf[n++] = (char)(0xF0 | (cp >> 18));
        buf[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[n++] = (char)(0x80 | (cp & 0x3F));
    }
    return n;
}

/* chars() -> the text typed since the last frame */
static int h_chars(lua_State *L) {
    char buf[256];
    int n = 0;
    for (int cp = GetCharPressed(); cp > 0; cp = GetCharPressed()) {
        if (n > (int)sizeof(buf) - 5) break;
        n = put_utf8(buf, n, cp);
    }
    lua_pushlstring(L, buf, (size_t)n);
    return 1;
}

/* key(code) -> true if the key was pressed this frame */
static int h_key(lua_State *L) {
    lua_pushboolean(L, IsKeyPressed((int)luaL_checkinteger(L, 1)));
    return 1;
}

/* clip(x, y, w, h) / unclip() — scissor for scroll views and canvases */
static int h_clip(lua_State *L) {
    BeginScissorMode((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2),
                     (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4));
    return 0;
}

static int h_unclip(lua_State *L) {
    (void)L;
    EndScissorMode();
    return 0;
}

/* line(x1, y1, x2, y2, thickness, color) */
static int h_line(lua_State *L) {
    Vector2 a = {(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)};
    Vector2 b = {(float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)};
    float thick = (float)luaL_optnumber(L, 5, 1.0);
    DrawLineEx(a, b, thick, rgb(L, 6, 255));
    return 0;
}

/* circle(x, y, radius, color) */
static int h_circle(lua_State *L) {
    Vector2 c = {(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)};
    DrawCircleV(c, (float)luaL_checknumber(L, 3), rgb(L, 4, 255));
    return 0;
}

/* wheel() -> mouse-wheel movement this frame */
static int h_wheel(lua_State *L) {
    (void)L;
    lua_pushnumber(L, GetMouseWheelMove());
    return 1;
}

/* dt() -> seconds since the last frame */
static int h_dt(lua_State *L) {
    (void)L;
    lua_pushnumber(L, GetFrameTime());
    return 1;
}

static const luaL_Reg host_fns[] = {
    {"open", h_open}, {"should_close", h_should_close}, {"size", h_size},
    {"textfield", h_textfield},
    {"begin", h_begin}, {"finish", h_finish},
    {"rect", h_rect}, {"outline", h_outline}, {"text", h_text},
    {"line", h_line}, {"circle", h_circle},
    {"measure", h_measure}, {"mouse", h_mouse},
    {"chars", h_chars}, {"key", h_key}, {"wheel", h_wheel}, {"dt", h_dt},
    {"clip", h_clip}, {"unclip", h_unclip}, {NULL, NULL},
};

static int report(lua_State *L) {
    fprintf(stderr, "lui: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: lui_host <chunk.lua> [lui.lua] [plugin.lua ...]\n");
        return 1;
    }
    const char *runtime = argc > 2 ? argv[2] : "lui.lua";

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_newtable(L);
    luaL_setfuncs(L, host_fns, 0);
    lua_setglobal(L, "lui_host");

    /* lui.lua runs first (it reads lui_host) and its result is the module. */
    if (luaL_loadfile(L, runtime) || lua_pcall(L, 0, 1, 0)) {
        report(L);
        return 1;
    }
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushvalue(L, -3);
    lua_setfield(L, -2, "lui");
    lua_pop(L, 3);

    /* Plugins: extra Lua files (argv[3..]) that call lui.define(...). */
    for (int i = 3; i < argc; i++) {
        if (luaL_loadfile(L, argv[i]) || lua_pcall(L, 0, 0, 0)) {
            report(L);
            return 1;
        }
    }

    /* Build the widget tree. */
    if (luaL_loadfile(L, argv[1]) || lua_pcall(L, 0, 0, 0)) {
        report(L);
        return 1;
    }

    lua_getglobal(L, "require");
    lua_pushliteral(L, "lui");
    lua_call(L, 1, 1);
    lua_getfield(L, -1, "run");
    if (lua_pcall(L, 0, 0, 0)) {
        report(L);
        return 1;
    }
    lua_pop(L, 1);

    for (;;) {
        lua_getglobal(L, "require");
        lua_pushliteral(L, "lui");
        lua_call(L, 1, 1);
        lua_getfield(L, -1, "frame");
        if (lua_pcall(L, 0, 1, 0)) {
            report(L);
            break;
        }
        int keep_going = lua_toboolean(L, -1);
        lua_pop(L, 2);
        if (!keep_going) break;
    }

    if (have_fonts) {
        UnloadFont(regular);
        if (bold.texture.id != regular.texture.id) UnloadFont(bold);
    }
    CloseWindow();
    lua_close(L);
    return 0;
}
