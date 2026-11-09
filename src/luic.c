/* luic — compiles a .lui file (custom XML markup with Lua, see README.md)
 * into a compact Lua chunk that builds the widget tree through the `lui`
 * runtime.  Output is minified: no indentation or blank lines, generated
 * names are short and scrambled, and the runtime is bound to one letter.
 *
 *   ./bin/luic counter.lui            writes counter.lui.lua
 *   ./bin/luic counter.lui out.lua    writes out.lua
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_MAX_LEN 128

/* ── growable string ───────────────────────────────────────────────── */

typedef struct {
    char *s;
    size_t len, cap;
} Buf;

static void buf_putn(Buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2;
        b->s = realloc(b->s, b->cap);
        if (!b->s) { perror("luic"); exit(1); }
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = '\0';
}

static void buf_puts(Buf *b, const char *s) { buf_putn(b, s, strlen(s)); }
static void buf_putc(Buf *b, char c) { buf_putn(b, &c, 1); }
static const char *buf_str(Buf *b) { return b->s ? b->s : ""; }

/* Appends a Lua string literal for s[0..n). */
static void lua_str(Buf *b, const char *s, size_t n) {
    buf_putc(b, '"');
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\') buf_puts(b, "\\\\");
        else if (s[i] == '"') buf_puts(b, "\\\"");
        else if (s[i] == '\n') buf_puts(b, "\\n");
        else buf_putc(b, s[i]);
    }
    buf_putc(b, '"');
}

/* ── source cursor ─────────────────────────────────────────────────── */

static const char *src;
static size_t src_len, pos;
static int line = 1;
static int counter;
static Buf out;

static void fail(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "luic: line %d: ", line);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(1);
}

static char look(size_t k) { return pos + k < src_len ? src[pos + k] : '\0'; }

static void step(void) {
    if (pos < src_len) {
        if (src[pos] == '\n') line++;
        pos++;
    }
}

static bool at(const char *s) {
    size_t n = strlen(s);
    return pos + n <= src_len && memcmp(src + pos, s, n) == 0;
}

static void skip_ws(void) {
    while (look(0) && strchr(" \t\r\n", look(0))) step();
}

static void expect(char c) {
    skip_ws();
    if (look(0) != c) fail("expected '%c' but found '%c'", c, look(0) ? look(0) : ' ');
    step();
}

static void skip_comment(void) {
    while (!at("-->")) {
        if (pos >= src_len) fail("unterminated comment");
        step();
    }
    pos += 3;
}

static void skip_to_gt(void) {
    while (look(0) && look(0) != '>') step();
    expect('>');
}

static bool is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static void read_name(char *dst) {
    size_t n = 0;
    while (is_name_char(look(0))) {
        if (n + 1 >= NAME_MAX_LEN) fail("name too long");
        dst[n++] = look(0);
        step();
    }
    dst[n] = '\0';
}

static void read_number(Buf *dst) {
    size_t start = dst->len;
    while (look(0) && strchr("0123456789.-", look(0))) {
        buf_putc(dst, look(0));
        step();
    }
    if (dst->len == start) fail("expected a number");
}

/* `"..."`: appends the raw text between the quotes. */
static void read_quoted(Buf *dst) {
    step();
    while (look(0) != '"') {
        if (!look(0)) fail("unterminated string");
        buf_putc(dst, look(0));
        step();
    }
    step();
}

/* `{...}`: appends the Lua expression between the braces. */
static void read_braced(Buf *dst) {
    step();
    int depth = 1;
    while (depth > 0) {
        if (!look(0)) fail("unterminated '{'");
        if (look(0) == '{') depth++;
        if (look(0) == '}') depth--;
        if (depth > 0) buf_putc(dst, look(0));
        step();
    }
}

/* ── output ────────────────────────────────────────────────────────── */

/* Scrambled short name for a generated local: _ followed by letters. */
static void fresh(char *dst) {
    static const char glyphs[] = "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM";
    int n = ++counter;
    size_t k = 0;
    dst[k++] = '_';
    while (n > 0) {
        dst[k++] = glyphs[n % 52];
        n /= 52;
    }
    dst[k] = '\0';
}

/* Text child: plain text sets .text once; `{expr}` pieces make the text
 * a live binding that the runtime re-evaluates every frame. */
static void emit_text(const char *node, const char *raw) {
    bool dyn = strchr(raw, '{') != NULL;
    Buf parts = {0};
    size_t n = strlen(raw), lit = 0;
    for (size_t i = 0; i < n;) {
        if (raw[i] == '{') {
            if (i > lit) {
                if (parts.len) buf_puts(&parts, "..");
                lua_str(&parts, raw + lit, i - lit);
            }
            size_t j = i + 1;
            while (j < n && raw[j] != '}') j++;
            if (parts.len) buf_puts(&parts, "..");
            buf_puts(&parts, "tostring(");
            buf_putn(&parts, raw + i + 1, j - i - 1);
            buf_puts(&parts, ")");
            i = lit = j + 1;
        } else {
            i++;
        }
    }
    if (n > lit) {
        if (parts.len) buf_puts(&parts, "..");
        lua_str(&parts, raw + lit, n - lit);
    }
    if (!parts.len) buf_puts(&parts, "\"\"");

    if (dyn) {
        buf_puts(&out, "L.bindtext(");
        buf_puts(&out, node);
        buf_puts(&out, ",function()return ");
        buf_puts(&out, parts.s);
        buf_puts(&out, " end)\n");
    } else {
        buf_puts(&out, node);
        buf_puts(&out, ".text=");
        buf_puts(&out, parts.s);
        buf_putc(&out, '\n');
    }
    free(parts.s);
}

/* ── grammar ───────────────────────────────────────────────────────── */

static void parse_children(const char *node, const char *tag);

/* Appends `node.key=`. */
static void emit_field(const char *node, const char *key) {
    buf_puts(&out, node);
    buf_putc(&out, '.');
    buf_puts(&out, key);
    buf_putc(&out, '=');
}

static void parse_attr(const char *node) {
    char key[NAME_MAX_LEN];
    read_name(key);
    if (!key[0]) fail("bad attribute in <%s>", node);
    skip_ws();
    if (look(0) != '=') {
        emit_field(node, key);
        buf_puts(&out, "true\n");
        return;
    }
    step();
    skip_ws();
    Buf v = {0};
    if (look(0) == '"') {
        read_quoted(&v);
        emit_field(node, key);
        lua_str(&out, buf_str(&v), v.len);
        buf_putc(&out, '\n');
    } else if (look(0) == '{') {
        read_braced(&v);
        buf_puts(&out, "L.bind("); buf_puts(&out, node); buf_putc(&out, ',');
        lua_str(&out, key, strlen(key));
        buf_puts(&out, ",function()return "); buf_puts(&out, buf_str(&v)); buf_puts(&out, " end)\n");
    } else {
        read_number(&v);
        emit_field(node, key);
        buf_puts(&out, buf_str(&v));
        buf_putc(&out, '\n');
    }
    free(v.s);
}

/* Attributes up to '>' or '/>'.  Returns true for a self-closing tag. */
static bool parse_attrs(const char *node) {
    for (;;) {
        skip_ws();
        if (at("/>")) {
            pos += 2;
            return true;
        }
        if (look(0) == '>') {
            step();
            return false;
        }
        parse_attr(node);
    }
}

/* `<on EVENT> lua </on>` inside an element (at "<on ").  The body is
 * minified: indentation and blank lines are dropped. */
static void parse_on(const char *node) {
    char ev[NAME_MAX_LEN];
    pos += 3;
    skip_ws();
    read_name(ev);
    expect('>');
    Buf body = {0};
    bool line_start = true, newline = false;
    while (!at("</on>")) {
        if (pos >= src_len) fail("unterminated <on %s>", ev);
        char c = look(0);
        if (line_start && (c == ' ' || c == '\t' || c == '\r')) {
            step();
        } else if (c == '\n') {
            newline = !line_start;
            line_start = true;
            step();
        } else {
            if (newline) {
                buf_putc(&body, '\n');
                newline = false;
            }
            buf_putc(&body, c);
            step();
            line_start = false;
        }
    }
    pos += 5;
    buf_puts(&out, "L.on("); buf_puts(&out, node); buf_putc(&out, ',');
    lua_str(&out, ev, strlen(ev));
    buf_puts(&out, ",function(self,g,dt)");
    buf_puts(&out, buf_str(&body));
    buf_puts(&out, "\nend)\n");
    free(body.s);
}

/* `<tag ...> children </tag>`, starting at '<'.  parent == "" for a root. */
static void parse_element(const char *binding, const char *parent) {
    char tag[NAME_MAX_LEN], node[NAME_MAX_LEN];
    step();
    read_name(tag);
    if (!tag[0]) fail("expected a tag name");
    if (binding[0]) snprintf(node, sizeof node, "%s", binding);
    else fresh(node);

    buf_puts(&out, "local "); buf_puts(&out, node); buf_puts(&out, "=L.node(");
    lua_str(&out, tag, strlen(tag));
    buf_puts(&out, ")\n");
    if (!parent[0]) {
        buf_puts(&out, "L.root("); buf_puts(&out, node); buf_puts(&out, ")\n");
    } else {
        buf_puts(&out, "L.add("); buf_puts(&out, parent); buf_putc(&out, ',');
        buf_puts(&out, node); buf_puts(&out, ")\n");
    }
    if (!parse_attrs(node)) parse_children(node, tag);
}

/* `local NAME = <tag> ... </tag>`, starting at 'local'. */
static void parse_local(const char *parent) {
    char name[NAME_MAX_LEN];
    pos += 6;
    skip_ws();
    read_name(name);
    expect('=');
    skip_ws();
    if (look(0) != '<') fail("expected <tag> after 'local %s ='", name);
    parse_element(name, parent);
}

/* True when the text at pos is `local NAME = <`.  Leaves pos and line alone. */
static bool local_element_ahead(void) {
    if (!at("local ")) return false;
    size_t p = pos;
    int l = line;
    char name[NAME_MAX_LEN];
    pos += 6;
    skip_ws();
    read_name(name);
    skip_ws();
    bool res = false;
    if (name[0] && look(0) == '=') {
        step();
        skip_ws();
        res = look(0) == '<';
    }
    pos = p;
    line = l;
    return res;
}

static void parse_children(const char *node, const char *tag) {
    for (;;) {
        skip_ws();
        if (pos >= src_len) fail("unterminated <%s>", tag);
        if (at("</")) {
            char close[NAME_MAX_LEN];
            pos += 2;
            read_name(close);
            expect('>');
            if (strcmp(close, tag) != 0) fail("expected </%s> but found </%s>", tag, close);
            return;
        }
        if (at("<!--")) {
            skip_comment();
        } else if (at("<on ")) {
            parse_on(node);
        } else if (at("<")) {
            parse_element("", node);
        } else if (look(0) == '"') {
            Buf raw = {0};
            read_quoted(&raw);
            emit_text(node, buf_str(&raw));
            free(raw.s);
        } else if (local_element_ahead()) {
            parse_local(node);
        } else {
            fail("unexpected '%c' in <%s>", look(0), tag);
        }
    }
}

/* A top-level Lua line, from its first non-blank column to the end of the line. */
static void copy_line(void) {
    size_t start = pos;
    while (pos < src_len && src[pos] != '\n') pos++;
    buf_putn(&out, src + start, pos - start);
    buf_putc(&out, '\n');
}

static void parse_top(void) {
    for (;;) {
        skip_ws();
        if (pos >= src_len) return;
        if (at("<lui")) {
            pos += 4;
            buf_puts(&out, "L.cfg={}\n");
            parse_attrs("L.cfg");
        } else if (at("</lui")) {
            skip_to_gt();
        } else if (at("<!--")) {
            skip_comment();
        } else if (local_element_ahead()) {
            parse_local("");
        } else if (at("<")) {
            parse_element("", "");
        } else {
            copy_line();
        }
    }
}

static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc((size_t)n + 1);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) { perror(path); exit(1); }
    data[n] = '\0';
    fclose(f);
    *len = (size_t)n;
    return data;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: luic <file.lui> [out.lua]\n");
        return 2;
    }
    char output[4096];
    if (argc > 2) snprintf(output, sizeof output, "%s", argv[2]);
    else snprintf(output, sizeof output, "%s.lua", argv[1]);

    src = read_file(argv[1], &src_len);
    buf_puts(&out, "local L=require\"lui\"\n");
    parse_top();

    FILE *f = fopen(output, "wb");
    if (!f) { perror(output); return 1; }
    fwrite(buf_str(&out), 1, out.len, f);
    fclose(f);
    printf("wrote %s\n", output);
    return 0;
}
