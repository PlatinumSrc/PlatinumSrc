#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
./cleanrelease.sh

[[ ${#} -eq 0 ]] && modules=(engine server editor) || modules=("${@}")

pkgrel() {
    "${ARCHIVER[@]}" "${OUTPUT}" "${FILES[@]}"
}

# Platforms that require disc images or similar file storage methods will not be included here (Emscripten, Dreamcast, etc.).
build_engine() {
    BASE="psrc_engine"

    ARCHIVER=_tar_u

    FILES=(psrc)
    OUTPUT="${BASE}_$(printf '%s' "${PLATDESC// /_}" | tr '[:upper:]' '[:lower:]')"
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    OUTPUT="${BASE}_$(printf '%s' "${PLATDESC32// /_}" | tr '[:upper:]' '[:lower:]')"
    buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y

    ARCHIVER=_zip_u

    FILES=(psrc.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' AR=gcc-ar inc.null=NUL

    FILES=(xiso/default.xbe)
    OUTPUT="${BASE}_nxdk"
    buildrel "${1}" "Xbox (NXDK)" "${@:2}" CROSS=nxdk
}
build_server() {
    BASE="psrc_server"

    ARCHIVER=_tar_u

    FILES=(psrc-server)
    OUTPUT="${BASE}_$(printf '%s' "${PLATDESC// /_}" | tr '[:upper:]' '[:lower:]')"
    buildrel "${1}" "${PLATDESC}" "${@:2}"

    ARCHIVER=_zip_u

    FILES=(psrc-server.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' AR=gcc-ar inc.null=NUL
}
build_editor() {
    BASE="psrc_editor"

    ARCHIVER=_tar_u

    FILES=(psrc-editor)
    OUTPUT="${BASE}_$(printf '%s' "${PLATDESC// /_}" | tr '[:upper:]' '[:lower:]')"
    buildrel "${1}" "${PLATDESC}" "${@:2}"

    ARCHIVER=_zip_u

    FILES=(psrc-editor.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' AR=gcc-ar inc.null=NUL
}

for i in "${modules[@]}"; do
    build() { "build_${i}" "${@}"; }
    buildmod "${i}"
done

}
