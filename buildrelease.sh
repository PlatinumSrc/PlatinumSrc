#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
./cleanrelease.sh

[[ ${#} -eq 0 ]] && modules=(engine server editor tools) || modules=("${@}")

pkgrel() {
    "${ARCHIVER[@]}" "${OUTPUT}" "${FILES[@]}"
}

FPLATDESC="$(printf '%s' "${PLATDESC// /_}" | tr '[:upper:]' '[:lower:]')"
FPLATDESC32="$(printf '%s' "${PLATDESC32// /_}" | tr '[:upper:]' '[:lower:]')"

debmake() {
    # debrun runs commands inside a debian oldoldstable chroot
    debrun '' make "${@}"
}

# Platforms that require disc images or similar file storage methods will not be included here (Emscripten, Dreamcast, etc.).
build_engine() {
    BASE="psrc_engine"

    ARCHIVER=_tar_u

    FILES=(psrc)
    MAKE=debmake
    OUTPUT="${BASE}_${FPLATDESC}"
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    OUTPUT="${BASE}_${FPLATDESC32}"
    buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
    unset MAKE

    ARCHIVER=_zip_u

    FILES=(psrc.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows 2000+ i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' inc.null=NUL
    OUTPUT="${BASE}_win9x_i686"
    buildrel "${1}" "Windows 95/98 i686" "${@:2}" CROSS=win32 M32=y USESDL1=y NOMT=y TOOLCHAIN='wine ' inc.null=NUL

    FILES=(xiso/default.xbe)
    OUTPUT="${BASE}_nxdk"
    buildrel "${1}" "Xbox (NXDK)" "${@:2}" CROSS=nxdk
}
build_server() {
    BASE="psrc_server"

    ARCHIVER=_tar_u

    FILES=(psrc-server)
    MAKE=debmake
    OUTPUT="${BASE}_${FPLATDESC}"
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    unset MAKE

    ARCHIVER=_zip_u

    FILES=(psrc-server.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' AR=gcc-ar inc.null=NUL
}
build_editor() {
    BASE="psrc_editor"

    ARCHIVER=_tar_u

    FILES=(psrc-editor)
    MAKE=debmake
    OUTPUT="${BASE}_${FPLATDESC}"
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    unset MAKE

    ARCHIVER=_zip_u

    FILES=(psrc-editor.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' AR=gcc-ar inc.null=NUL
}
build_tools() {
    BASE="psrc_tools"

    ARCHIVER=_tar

    FILES=(tools/platinum/platinum tools/ptftool/ptftool)
    MAKE=debmake
    OUTPUT="${BASE}_${FPLATDESC}"
    buildrel "${1}" "${PLATDESC}" -C tools "${@:2}"
    unset MAKE

    ARCHIVER=_zip

    FILES=(tools/platinum/platinum.exe tools/ptftool/ptftool.exe)
    OUTPUT="${BASE}_windows_x86_64"
    buildrel "${1}" "Windows x86_64" -C tools "${@:2}" TOOLCHAIN='x86_64-w64-mingw32-'
}

for i in "${modules[@]}"; do
    "build_${i}" "${i}" MODULE="${i}" || _exit
done

}
