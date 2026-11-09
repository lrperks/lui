/* lui_host — runs a luic-compiled chunk in a raylib window with Lua 5.3.
 * Dear ImGui (lui_imgui.cpp) draws every element; this file owns the window,
 * the frame loop, and background HTTP requests.
 *
 *   ./bin/lui_host app.lui.lua [lui.lua] [plugin.lua ...]
 */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

void lui_imgui_setup(void);
void lui_imgui_begin(void);
void lui_imgui_end(void);
void lui_imgui_lua(lua_State *L);

#define MIN_W 240
#define MIN_H 160

/* open(title, width, height, resizable = true) */
static int h_open(lua_State *L) {
    const char *title = luaL_checkstring(L, 1);
    int resizable = lua_isnoneornil(L, 4) || lua_toboolean(L, 4);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | (resizable ? FLAG_WINDOW_RESIZABLE : 0));
    InitWindow((int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), title);
    SetWindowMinSize(MIN_W, MIN_H);
    SetTargetFPS(60);
    lui_imgui_setup();
    return 0;
}

static int h_should_close(lua_State *L) {
    lua_pushboolean(L, WindowShouldClose());
    return 1;
}

/* begin(bg) / finish(): one frame, with ImGui's frame inside it */
static int h_begin(lua_State *L) {
    int c = (int)luaL_optinteger(L, 1, 0);
    BeginDrawing();
    ClearBackground((Color){(c >> 16) & 255, (c >> 8) & 255, c & 255, 255});
    lui_imgui_begin();
    return 0;
}

static int h_finish(lua_State *L) {
    (void)L;
    lui_imgui_end();
    EndDrawing();
    return 0;
}

/* dt() -> seconds since the last frame */
static int h_dt(lua_State *L) {
    lua_pushnumber(L, GetFrameTime());
    return 1;
}

/* Background HTTP: http_start(url) -> job id (or nil), http_poll(id) ->
 * "pending" | "ok", body | "error", message.  Each job runs curl on its own
 * thread so the window keeps drawing while the request is in flight. */
#define MAX_JOBS 16
enum { JOB_FREE, JOB_RUNNING, JOB_DONE_OK, JOB_DONE_ERR };
typedef struct {
    int state;
    char *body;
    size_t len;
    pthread_t thread;
} Job;
static Job jobs[MAX_JOBS];
static pthread_mutex_t jobs_lock = PTHREAD_MUTEX_INITIALIZER;

static void *run_job(void *arg) {
    Job *job = arg;
    char cmd[2200];   /* the command is stored in body until the thread runs */
    pthread_mutex_lock(&jobs_lock);
    snprintf(cmd, sizeof cmd, "%s", job->body);
    free(job->body);
    job->body = NULL;
    pthread_mutex_unlock(&jobs_lock);

    FILE *p = popen(cmd, "r");
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (p && buf) {
        size_t n;
        while ((n = fread(buf + len, 1, cap - len, p)) > 0) {
            len += n;
            if (len == cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) break; }
        }
    }
    int ok = p && pclose(p) == 0 && buf && len > 0;
    pthread_mutex_lock(&jobs_lock);
    job->body = ok ? buf : NULL;
    job->len = len;
    job->state = ok ? JOB_DONE_OK : JOB_DONE_ERR;
    if (!ok) free(buf);
    pthread_mutex_unlock(&jobs_lock);
    return NULL;
}

static int h_http_start(lua_State *L) {
    const char *url = luaL_checkstring(L, 1);
    if (strchr(url, '\'') != NULL) return luaL_error(L, "http_start: quote in url");
    char cmd[2200];
    snprintf(cmd, sizeof cmd, "curl -sfL --max-time 10 '%s' 2>/dev/null", url);
    pthread_mutex_lock(&jobs_lock);
    int id = -1;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].state == JOB_FREE) { id = i; break; }
    }
    if (id >= 0) {
        jobs[id].state = JOB_RUNNING;
        jobs[id].body = strdup(cmd);
    }
    pthread_mutex_unlock(&jobs_lock);
    if (id < 0) { lua_pushnil(L); return 1; }
    if (pthread_create(&jobs[id].thread, NULL, run_job, &jobs[id]) != 0) {
        pthread_mutex_lock(&jobs_lock);
        jobs[id].state = JOB_FREE;
        pthread_mutex_unlock(&jobs_lock);
        lua_pushnil(L);
        return 1;
    }
    pthread_detach(jobs[id].thread);
    lua_pushinteger(L, id + 1);
    return 1;
}

static int h_http_poll(lua_State *L) {
    int id = (int)luaL_checkinteger(L, 1) - 1;
    if (id < 0 || id >= MAX_JOBS) return luaL_error(L, "http_poll: bad id");
    pthread_mutex_lock(&jobs_lock);
    Job *job = &jobs[id];
    if (job->state == JOB_RUNNING) {
        pthread_mutex_unlock(&jobs_lock);
        lua_pushliteral(L, "pending");
        return 1;
    }
    if (job->state == JOB_DONE_OK) {
        lua_pushliteral(L, "ok");
        lua_pushlstring(L, job->body, job->len);
    } else {
        lua_pushliteral(L, "error");
        lua_pushliteral(L, "request failed");
    }
    free(job->body);
    job->body = NULL;
    job->state = JOB_FREE;
    pthread_mutex_unlock(&jobs_lock);
    return 2;
}

static const luaL_Reg host_fns[] = {
    {"open", h_open}, {"should_close", h_should_close},
    {"begin", h_begin}, {"finish", h_finish}, {"dt", h_dt},
    {"http_start", h_http_start}, {"http_poll", h_http_poll},
    {NULL, NULL},
};

static int report(lua_State *L) {
    fprintf(stderr, "lui: %s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    return 1;
}

/* Calls lui.<name>() and returns its boolean result (false on error). */
static int call_lui(lua_State *L, const char *name) {
    lua_getglobal(L, "require");
    lua_pushliteral(L, "lui");
    lua_call(L, 1, 1);
    lua_getfield(L, -1, name);
    if (lua_pcall(L, 0, 1, 0)) {
        report(L);
        lua_pop(L, 1);
        return 0;
    }
    int result = lua_toboolean(L, -1);
    lua_pop(L, 2);
    return result;
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
    lui_imgui_lua(L);

    /* lui.lua runs first and its result becomes the module "lui". */
    if (luaL_loadfile(L, runtime) || lua_pcall(L, 0, 1, 0)) return report(L);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushvalue(L, -3);
    lua_setfield(L, -2, "lui");
    lua_pop(L, 3);

    /* Plugins: extra Lua files (argv[3..]) that call lui.define(...). */
    for (int i = 3; i < argc; i++)
        if (luaL_loadfile(L, argv[i]) || lua_pcall(L, 0, 0, 0)) return report(L);

    /* Build the element tree, then open the window. */
    if (luaL_loadfile(L, argv[1]) || lua_pcall(L, 0, 0, 0)) return report(L);
    call_lui(L, "run");

    /* LUI_SHOT=file.png: save a screenshot after LUI_SHOT_FRAMES frames
     * (default 120) and exit.  Used to check layouts without a person looking. */
    const char *shot = getenv("LUI_SHOT");
    int shot_at = getenv("LUI_SHOT_FRAMES") ? atoi(getenv("LUI_SHOT_FRAMES")) : 120;
    for (int frame = 1;; frame++) {
        if (shot && frame == shot_at) {
            TakeScreenshot(shot);
            break;
        }
        if (!call_lui(L, "frame")) break;
    }

    CloseWindow();
    lua_close(L);
    return 0;
}
