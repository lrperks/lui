#!/bin/sh
# Compiler tests.
#   - every examples/*.lui must compile to exactly tests/expected/<name>.lua
#     and the result must be valid Lua;
#   - every tests/errors/*.lui must be rejected with a "luic: line N:" message.
# Regenerate the expected files after an intended output change:
#   for f in examples/*.lui; do bin/luic "$f" "tests/expected/$(basename "$f" .lui).lua"; done
set -u
cd "$(dirname "$0")/.."
LUA=$(command -v lua5.3 || command -v lua || true)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fail=0

for src in examples/*.lui; do
    name=$(basename "$src" .lui)
    out="$tmp/$name.lua"
    if ! bin/luic "$src" "$out" >/dev/null; then
        echo "FAIL $name: did not compile"; fail=1; continue
    fi
    if ! cmp -s "$out" "tests/expected/$name.lua"; then
        echo "FAIL $name: output differs from tests/expected/$name.lua"
        diff "tests/expected/$name.lua" "$out" | head -10
        fail=1; continue
    fi
    if [ -n "$LUA" ] && ! "$LUA" -e "assert(loadfile('$out'))" 2>/dev/null; then
        echo "FAIL $name: output is not valid Lua"; fail=1; continue
    fi
    echo "ok   $name"
done

for src in tests/errors/*.lui; do
    name=$(basename "$src" .lui)
    if msg=$(bin/luic "$src" "$tmp/err.lua" 2>&1); then
        echo "FAIL error/$name: was accepted"; fail=1
    elif echo "$msg" | grep -q "^luic: line [0-9]*:"; then
        echo "ok   error/$name"
    else
        echo "FAIL error/$name: unexpected message: $msg"; fail=1
    fi
done

[ "$fail" -eq 0 ] && echo "all tests passed"
exit "$fail"
