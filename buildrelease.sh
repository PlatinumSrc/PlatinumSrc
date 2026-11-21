#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
./cleanrelease.sh

[[ ${#} -eq 0 ]] && modules=(engine server editor tools) || modules=("${@}")

buildrel() {
    [ -z "${2}" ] && inf "Building ${1}..." || inf "Building ${1} for ${2}..."
    local _MAKE="${MAKE}"
    [ -z "${_MAKE}" ] && _MAKE=make
    "${_MAKE}" "${@:3}" distclean 1> /dev/null || _exit
    RESPONSE=""
    while ! "${_MAKE}" "${@:3}" "-j${NJOBS}" 1> /dev/null; do
        while [[ -z "${RESPONSE}" ]]; do
            ask "${TBQ}Build failed. Retry?${TRQ} (${TBQ}Y${TRQ}es/${TBQ}N${TRQ}o/${TBQ}S${TRQ}kip/${TBQ}C${TRQ}lean)" RESPONSE
            case "${RESPONSE,,}" in
                y | yes | n | no | s | skip)
                    break
                    ;;
                c | clean)
                    "${_MAKE}" "${@:3}" distclean 1> /dev/null || _exit
                    ;;
                *)
                    RESPONSE=""
                    ;;
            esac
        done
        case "${RESPONSE,,}" in
            n | no)
                RESPONSE="n"
                break
                ;;
            s | skip)
                RESPONSE="s"
                break
                ;;
            *)
                RESPONSE=""
                ;;
        esac
    done
    [[ "${RESPONSE}" == "n" ]] || [[ "${RESPONSE}" == "s" ]] || pkgrel || _exit
    "${_MAKE}" "${@:3}" distclean 1> /dev/null || _exit
    [[ ! "${RESPONSE}" == "n" ]] || _exit 1
}
pkgrel() {
    "${ARCHIVER[@]}" "${OUTPUT}" "${FILES[@]}"
}

debmake() {
    # debrun runs commands inside a debian buster chroot
    debrun make "${@}"
}

buildplat_linux() {
    OUTPUT="${BASE}_linux_x86_64"
    MAKE=debmake buildrel "${1}" 'Linux x86_64' "${@}"
    OUTPUT="${BASE}_linux_i686"
    MAKE=debmake buildrel "${1}" 'Linux i686' "${@}" M32=y
    OUTPUT="${BASE}_linux_aarch64"
    MAKE=debmake buildrel "${1}" 'Linux aarch64' "${@}" PLATFORM='Linux_aarch64' TOOLCHAIN=aarch64-linux-gnu- CC=gcc
    OUTPUT="${BASE}_linux_armv7l"
    MAKE=debmake buildrel "${1}" 'Linux armv7l' "${@}" PLATFORM='Linux_armv7l' TOOLCHAIN=arm-linux-gnueabihf- CC=gcc
}
buildplat_win32() {
    OUTPUT="${BASE}_windows_x86_64"
    buildrel "${1}" 'Windows x86_64' "${@}" CROSS=win32 TOOLCHAIN=x86_64-w64-mingw32-
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" 'Windows i686' "${@}" CROSS=win32 M32=y TOOLCHAIN='wine '
}
buildplat_oldwin() {
    OUTPUT="${BASE}_windows_x86_64"
    buildrel "${1}" 'Windows x86_64' "${@}" CROSS=win32 TOOLCHAIN=x86_64-w64-mingw32-
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" 'Windows 2000+ i686' "${@}" CROSS=win32 M32=y TOOLCHAIN='wine '
    OUTPUT="${BASE}_win98_i686"
    buildrel "${1}" 'Windows 98 i686' "${@}" CROSS=win32 M32=y USESDL1=y NOMT=y TOOLCHAIN='wine '
}

# Platforms that require disc images or similar file storage methods will not be included here (Emscripten, Dreamcast, etc.).
build_engine() {
    BASE='psrc_engine'

    ARCHIVER=_tar_u

    FILES=(psrc)
    buildplat_linux "${@:2}"

    ARCHIVER=_zip_u

    FILES=(psrc.exe)
    buildplat_oldwin "${@:2}"

    FILES=(xiso/default.xbe)
    OUTPUT="${BASE}_nxdk"
    buildrel "${1}" 'Xbox (NXDK)' "${@:2}" CROSS=nxdk
}
build_server() {
    BASE="psrc_server"

    ARCHIVER=_tar_u

    FILES=(psrc-server)
    buildplat_linux "${@:2}"

    ARCHIVER=_zip_u

    FILES=(psrc-server.exe)
    buildplat_win32 "${@:2}"
}
build_editor() {
    BASE="psrc_editor"

    ARCHIVER=_tar_u

    FILES=(psrc-editor)
    buildplat_linux "${@:2}"

    ARCHIVER=_zip_u

    FILES=(psrc-editor.exe)
    buildplat_oldwin "${@:2}"
}
build_tools() {
    BASE="psrc_tools"

    ARCHIVER=_tar

    FILES=(
        tools/crc/crc
        tools/paftool/paftool
        tools/pkdtool/pkdtool
        tools/platinum/platinum
        tools/ptftool/ptftool
    )
    OUTPUT="${BASE}_linux_x86_64"
    MAKE=debmake buildrel "${1}" 'Linux x86_64' -C tools "${@:2}"
    OUTPUT="${BASE}_linux_i686"
    MAKE=debmake buildrel "${1}" 'Linux i686' -C tools "${@:2}" CFLAGS+=-m32 LDFLAGS+=-m32
    OUTPUT="${BASE}_linux_aarch64"
    MAKE=debmake buildrel "${1}" 'Linux aarch64' -C tools "${@:2}" TOOLCHAIN=aarch64-linux-gnu- CC=gcc
    OUTPUT="${BASE}_linux_armv7l"
    MAKE=debmake buildrel "${1}" 'Linux armv7l' -C tools "${@:2}" TOOLCHAIN=arm-linux-gnueabihf- CC=gcc

    ARCHIVER=_zip

    FILES=(
        tools/crc/crc.exe
        tools/paftool/paftool.exe
        tools/pkdtool/pkdtool.exe
        tools/platinum/platinum.exe
        tools/ptftool/ptftool.exe
    )
    OUTPUT="${BASE}_windows_x86_64"
    buildrel "${1}" 'Windows x86_64' -C tools "${@:2}" OS=Windows_NT TOOLCHAIN='wine ' CC=gcc null=NUL
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" 'Windows i686' -C tools "${@:2}" OS=Windows_NT TOOLCHAIN='wine ' CC=gcc CFLAGS+=-m32 LDFLAGS+=-m32 null=NUL
}

for i in "${modules[@]}"; do
    "build_${i}" "${i}" MODULE="${i}" deps= || _exit
done

}
