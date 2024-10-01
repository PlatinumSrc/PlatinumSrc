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
    # debrun runs commands inside a debian buster chroot
    debrun '' make "${@}"
}

# Platforms that require disc images or similar file storage methods will not be included here (Emscripten, Dreamcast, etc.).
build_engine() {
    BASE="psrc_engine"

    ARCHIVER=_tar_u

    FILES=(psrc)
    OUTPUT="${BASE}_${FPLATDESC}"
    MAKE=debmake buildrel "${1}" "${PLATDESC}" "${@:2}"
    OUTPUT="${BASE}_${FPLATDESC32}"
    MAKE=debmake buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
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
    OUTPUT="${BASE}_${FPLATDESC}"
    MAKE=debmake buildrel "${1}" "${PLATDESC}" "${@:2}"
    unset MAKE

    ARCHIVER=_zip_u

    FILES=(psrc-server.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' inc.null=NUL
}
build_editor() {
    BASE="psrc_editor"

    ARCHIVER=_tar_u

    FILES=(psrc-editor)
    OUTPUT="${BASE}_${FPLATDESC}"
    MAKE=debmake buildrel "${1}" "${PLATDESC}" "${@:2}"
    unset MAKE

    ARCHIVER=_zip_u

    FILES=(psrc-editor.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine ' inc.null=NUL
}
build_tools() {
    BASE="psrc_tools"

    ARCHIVER=_tar

    FILES=(tools/platinum/platinum tools/ptftool/ptftool)
    OUTPUT="${BASE}_${FPLATDESC}"
    MAKE=debmake buildrel "${1}" "${PLATDESC}" -C tools "${@:2}"
    unset MAKE

    ARCHIVER=_zip

    FILES=(tools/platinum/platinum.exe tools/ptftool/ptftool.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" -C tools "${@:2}" OS=Windows_NT CC=gcc CFLAGS+=-m32 TOOLCHAIN='wine ' null=NUL
}

for i in "${modules[@]}"; do
    "build_${i}" "${i}" MODULE="${i}" || _exit
done

}
