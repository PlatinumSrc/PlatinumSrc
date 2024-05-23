#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
./cleanrelease.sh

[[ ${#} -eq 0 ]] && modules=(engine server editor) || modules=("${@}")

pkgrel() {
    if [ -z "${1}" ]; then
        "${ARCHIVER[@]}" "${OUTPUT}" "${FILES[@]}"
    else
        "${ARCHIVER[@]}" "${OUTPUT} ${1}" "${FILES[@]}"
    fi
}

# Platforms that require disc images or similar file storage methods will not be included here (Emscripten, Dreamcast, etc.).
build_engine() {
    OUTPUT="PlatinumSrc Engine"
    ARCHIVER=_tar_u
    FILES=(psrc)
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
    ARCHIVER=_zip_u
    FILES=(psrc.exe)
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine i686-w64-mingw32-' AR=gcc-ar inc.null=NUL
    FILES=(xiso/default.xbe)
    buildrel "${1}" "Xbox (NXDK)" "${@:2}" CROSS=nxdk
}
build_server() {
    OUTPUT="PlatinumSrc Server"
    ARCHIVER=_tar_u
    FILES=(psrc-server)
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    ARCHIVER=_zip_u
    FILES=(psrc-server.exe)
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine i686-w64-mingw32-' AR=gcc-ar inc.null=NUL
}
build_editor() {
    OUTPUT="PlatinumSrc Editor"
    ARCHIVER=_tar_u
    FILES=(psrc-editor)
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    ARCHIVER=_zip_u
    FILES=(psrc-editor.exe)
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine i686-w64-mingw32-' AR=gcc-ar inc.null=NUL
}

for i in "${modules[@]}"; do
    build() { "build_${i}" "${@}"; }
    buildmod "${i}"
done

}
