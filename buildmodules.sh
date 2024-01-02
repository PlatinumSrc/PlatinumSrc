#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
rm -rf PlatinumSrc*.tar.gz PlatinumSrc*.zip

[[ ${#} -eq 0 ]] && modules=(engine server editor) || modules=("${@}")

build_engine() {
    pkgrel() { _tar_u "PlatinumSrc Engine ${PLATPATH}" psrc; }
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    pkgrel() { _tar_u "PlatinumSrc Engine ${PLATPATH32}" psrc; }
    buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
    pkgrel() { _zip_u "PlatinumSrc Engine Windows i686" psrc.exe; }
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' inc.null=NUL
    pkgrel() { _zip_u "PlatinumSrc Engine Xbox (NXDK)" PlatinumSrc.xiso.iso; }
    buildrel "${1}" "Xbox (NXDK)" "${@:2}" CROSS=nxdk
    pkgrel() { _zip_u "PlatinumSrc Engine Emscripten" index.html psrc.js psrc.wasm; }
    buildrel "${1}" "Emscripten" "${@:2}" CROSS=emscr
}
build_server() {
    pkgrel() { _tar_u "PlatinumSrc Server ${PLATPATH}" psrc-server; }
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    pkgrel() { _zip_u "PlatinumSrc Server Windows i686" psrc-server.exe; }
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' inc.null=NUL
}
build_editor() {
    pkgrel() { _tar_u "PlatinumSrc Editor ${PLATPATH}" psrc-editor; }
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    pkgrel() { _zip_u "PlatinumSrc Editor Windows i686" psrc-editor.exe; }
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' inc.null=NUL
}

for i in "${modules[@]}"; do
    build() { "build_${i}" "${@}"; }
    buildmod "${i}"
done

}
