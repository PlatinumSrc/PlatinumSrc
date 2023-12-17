#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"

pkgrel() { :; }

build() {
    buildrel "${1}" "$(uname -o) $(uname -m)" "${@:2}"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' WINDRES='wine i686-w64-mingw32-windres' null=NUL
}
buildmod "engine"

build() {
    buildrel "${1}" "$(uname -o) $(uname -m)" "${@:2}"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' WINDRES='wine i686-w64-mingw32-windres' null=NUL
}
buildmod "server"

build() {
    buildrel "${1}" "$(uname -o) $(uname -m)" "${@:2}"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' WINDRES='wine i686-w64-mingw32-windres' null=NUL
}
buildmod "editor"

}
