#!/bin/bash

{

RENDERER_C="src/psrc/engine/renderer/gl.c"
GLAD_C="src/glad/glad.c"
GLAD_C_NEW="$(mktemp)"
WHITELIST="glGetString glGetStringi glGetIntegerv"

FUNCS="$(grep -Po '(\/\/.*)*(?<![\w\._])gl[A-Z][A-Za-z0-9]*(?=\s*\()' -- "$RENDERER_C" | grep -v '//' | sort -u) $WHITELIST"

r() { rm -f -- "$GLAD_C_NEW"; }
u() { r() { mv -f -- "$GLAD_C_NEW" "$GLAD_C"; }; u() { :; }; }
f1() {
    if [[ $l =~ [A-Za-z0-9][[:space:]]+glad_gl_load_[A-Za-z0-9_]+\(.+\{ ]]; then
        s="$(printf '%s\n' "$l" | grep -Po '(?<=\sglad_gl_load_|^glad_gl_load_)[A-Za-z0-9_]+(?=\()')"
        f() { f2; }
    fi
}
f2() {
    if [[ $l =~ \} ]]; then
        f() { f1; }
    elif [[ $l =~ load\( ]]; then
        if [[ $l =~ ^[[:space:]]*// ]]; then
            for i in $FUNCS; do
                if [[ $l =~ \"$i\" ]]; then
                    u
                    t="$(printf '%s\n' "$l" | grep -Po '\s+(?=\/)')"
                    l="$t$(printf '%s\n' "$l" | grep -Po '\s*(?<=\/\/).*')"
                    echo "Uncommented $i under $s"
                    break
                fi
            done
        else
            for i in $FUNCS ''; do
                [[ $l =~ \"$i\" ]] && break
                if [[ -z "$i" ]]; then
                    u
                    l="//$l"
                    echo "Commented out $(printf '%s\n' "$l" | grep -Po '(?<=")gl[A-Z][A-Za-z0-9]*(?=")') under $s"
                fi
            done
        fi
    fi
}
f() { f1; }

while IFS= read -r l; do
    f; printf '%s\n' "$l" >> "$GLAD_C_NEW"
done < "$GLAD_C"
r

}
