/* lui — run, build and watch .lui apps.
 *
 *   lui run app.lui        open the app
 *   lui watch app.lui      open it and reload on every save
 *   lui build app.lui      compile to app.lui.lua
 *   lui new app            create app.lui from a starter template
 *
 * The runtime (src/lui.lua) is built into the binary; Dear ImGui
 * (lui_imgui.cpp) draws every element.  This file owns the window, the frame
 * loop, background HTTP, and the command line.
 */
#include "luic.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <raylib.h>
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define LUI_VERSION "0.1.0"

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

/* The runtime, embedded by src/runtime.S.  LUI_RUNTIME=path/to/lui.lua loads
 * a file instead, for working on the runtime without rebuilding. */
extern const char lui_runtime[];
extern const unsigned long lui_runtime_size;

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

static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc((size_t)n + 1);
    if (data && fread(data, 1, (size_t)n, f) == (size_t)n) {
        data[n] = '\0';
        *len = (size_t)n;
    } else {
        free(data);
        data = NULL;
    }
    fclose(f);
    return data;
}

static int ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

/* Directory part of a path ("." if none), in out. */
static void dir_of(const char *path, char *out, size_t cap) {
    const char *slash = strrchr(path, '/');
    if (!slash) snprintf(out, cap, ".");
    else snprintf(out, cap, "%.*s", (int)(slash - path), path);
}

static int by_name(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Paths of the .lua files in a plugins/ folder next to the app, sorted.
 * Returns the count. */
static int list_plugins(const char *app, char paths[][1024], int max) {
    char dir[512], plugins[600];
    dir_of(app, dir, sizeof dir);
    snprintf(plugins, sizeof plugins, "%s/plugins", dir);
    DIR *d = opendir(plugins);
    if (!d) return 0;
    char *names[64];
    int n = 0;
    for (struct dirent *e; (e = readdir(d)) && n < max && n < 64;)
        if (ends_with(e->d_name, ".lua")) names[n++] = strdup(e->d_name);
    closedir(d);
    qsort(names, (size_t)n, sizeof names[0], by_name);
    for (int i = 0; i < n; i++) {
        snprintf(paths[i], 1024, "%s/%s", plugins, names[i]);
        free(names[i]);
    }
    return n;
}

/* Compiles app (.lui, or a precompiled .lua) to a Lua chunk; NULL on error. */
static char *load_app(const char *app, size_t *len) {
    size_t src_len;
    char *src = read_file(app, &src_len);
    if (!src) {
        fprintf(stderr, "lui: can't read %s\n", app);
        return NULL;
    }
    if (!ends_with(app, ".lui")) {
        *len = src_len;
        return src;
    }
    const char *error;
    char *lua = luic_compile(src, src_len, len, &error);
    free(src);
    if (!lua) fprintf(stderr, "lui: %s:%s\n", app, error + 5);   /* "line N: …" -> "file:N: …" */
    return lua;
}

/* Runs one app until its window closes.  Returns the process exit status. */
static int run_app(const char *app) {
    size_t len;
    char *chunk = load_app(app, &len);
    if (!chunk) return 1;

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    lua_newtable(L);
    luaL_setfuncs(L, host_fns, 0);
    lua_setglobal(L, "lui_host");
    lui_imgui_lua(L);

    /* The runtime runs first and its result becomes the module "lui". */
    const char *runtime = getenv("LUI_RUNTIME");
    int failed = runtime ? luaL_loadfile(L, runtime)
                         : luaL_loadbuffer(L, lui_runtime, lui_runtime_size, "=lui.lua");
    if (failed || lua_pcall(L, 0, 1, 0)) return report(L);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushvalue(L, -3);
    lua_setfield(L, -2, "lui");
    lua_pop(L, 3);

    /* Plugins (.lua files in plugins/ next to the app) can add elements. */
    static char plugins[32][1024];
    int nplugins = list_plugins(app, plugins, 32);
    for (int i = 0; i < nplugins; i++)
        if (luaL_loadfile(L, plugins[i]) || lua_pcall(L, 0, 0, 0)) return report(L);

    /* Build the element tree, then open the window. */
    char name[1100];
    const char *base = strrchr(app, '/');
    snprintf(name, sizeof name, "=%s", base ? base + 1 : app);
    if (luaL_loadbuffer(L, chunk, len, name) || lua_pcall(L, 0, 0, 0)) return report(L);
    free(chunk);
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

/* Newest modification time of the app and its plugins. */
static time_t stamp(const char *app) {
    struct stat st;
    time_t newest = stat(app, &st) == 0 ? st.st_mtime : 0;
    char plugins[32][1024];
    int n = list_plugins(app, plugins, 32);
    for (int i = 0; i < n; i++)
        if (stat(plugins[i], &st) == 0 && st.st_mtime > newest) newest = st.st_mtime;
    return newest;
}

static pid_t watched = 0;

/* Stopping `lui watch` also closes the app it is running. */
static void stop_watching(int sig) {
    if (watched > 0) kill(watched, SIGTERM);
    _exit(128 + sig);
}

/* Runs the app in a child process and restarts it whenever a file changes.
 * Closing the window ends watching; a broken file waits for the next save. */
static int watch_app(const char *app) {
    signal(SIGTERM, stop_watching);
    signal(SIGINT, stop_watching);
    for (;;) {
        time_t seen = stamp(app);
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            signal(SIGTERM, SIG_DFL);
            signal(SIGINT, SIG_DFL);
            _exit(run_app(app));
        }
        watched = pid;

        for (;;) {
            usleep(300 * 1000);
            int status;
            if (waitpid(pid, &status, WNOHANG) == pid) {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return 0;
                fprintf(stderr, "lui: waiting for changes to %s\n", app);
                while (stamp(app) == seen) usleep(300 * 1000);
                break;
            }
            if (stamp(app) != seen) {
                kill(pid, SIGTERM);
                waitpid(pid, &status, 0);
                break;
            }
        }
        fprintf(stderr, "lui: reloading %s\n", app);
    }
}

static int build_app(const char *app, const char *out_path) {
    size_t len;
    char *chunk = load_app(app, &len);
    if (!chunk) return 1;
    char def[1100];
    if (!out_path) {
        snprintf(def, sizeof def, "%s.lua", app);
        out_path = def;
    }
    FILE *f = fopen(out_path, "wb");
    if (!f) { perror(out_path); return 1; }
    fwrite(chunk, 1, len, f);
    fclose(f);
    printf("wrote %s\n", out_path);
    return 0;
}

static const char TEMPLATE[] =
    "<lui title=\"%s\" width=480 height=320>\n"
    "local count = 0\n"
    "\n"
    "<text size=22 weight=1>\"Hello from %s\"</text>\n"
    "<text>\"Edit %s and save: lui watch reloads it.\"</text>\n"
    "<separator/>\n"
    "<button>\n"
    "    \"Clicked {count} times\"\n"
    "    <on click>\n"
    "        count = count + 1\n"
    "    </on>\n"
    "</button>\n"
    "\n"
    "</lui>\n";

static int new_app(const char *name) {
    char path[1024], title[1024];
    snprintf(path, sizeof path, "%s%s", name, ends_with(name, ".lui") ? "" : ".lui");
    const char *base = strrchr(path, '/');
    snprintf(title, sizeof title, "%.*s", (int)(strlen(base ? base + 1 : path) - 4), base ? base + 1 : path);
    if (access(path, F_OK) == 0) {
        fprintf(stderr, "lui: %s already exists\n", path);
        return 1;
    }
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return 1; }
    fprintf(f, TEMPLATE, title, title, path);
    fclose(f);
    printf("created %s\nrun it with:  lui watch %s\n", path, path);
    return 0;
}

static void usage(FILE *to) {
    fprintf(to,
            "lui " LUI_VERSION " - Dear ImGui, written as markup\n"
            "\n"
            "usage:\n"
            "  lui run <app.lui>           open the app\n"
            "  lui watch <app.lui>         open it and reload on every save\n"
            "  lui build <app.lui> [out]   compile to Lua (default <app.lui>.lua)\n"
            "  lui new <name>              create <name>.lui from a template\n"
            "  lui version\n"
            "\n"
            "Plugins in a plugins/ folder next to the app are loaded first.\n");
}

int main(int argc, char **argv) {
    const char *cmd = argc > 1 ? argv[1] : "help";
    const char *arg = argc > 2 ? argv[2] : NULL;

    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version") || !strcmp(cmd, "-v")) {
        puts("lui " LUI_VERSION);
        return 0;
    }
    if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
        usage(stdout);
        return 0;
    }
    if (ends_with(cmd, ".lui")) return run_app(cmd);   /* `lui app.lui` is `lui run app.lui` */
    if (!arg) {
        usage(stderr);
        return 2;
    }
    if (!strcmp(cmd, "run")) return run_app(arg);
    if (!strcmp(cmd, "watch")) return watch_app(arg);
    if (!strcmp(cmd, "build")) return build_app(arg, argc > 3 ? argv[3] : NULL);
    if (!strcmp(cmd, "new")) return new_app(arg);
    fprintf(stderr, "lui: unknown command '%s'\n\n", cmd);
    usage(stderr);
    return 2;
}
