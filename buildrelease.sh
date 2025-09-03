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
            ask "${TBQ}Build failed. Retry?${TRQ} (${TBQ}Y${TRQ}es/${TBQ}N${TRQ}o/${TBQ}S${TRQ}kip/${TBQ}C${TRQ}lean): "
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

FPLATDESC="$(printf '%s' "${PLATDESC// /_}" | tr '[:upper:]' '[:lower:]')"
FPLATDESC32="$(printf '%s' "${PLATDESC32// /_}" | tr '[:upper:]' '[:lower:]')"

debmake() {
    # debrun runs commands inside a debian buster chroot
    debrun make "${@}"
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

    ARCHIVER=_zip_u

    FILES=(psrc.exe)
    OUTPUT="${BASE}_windows_x86_64"
    buildrel "${1}" "Windows x86_64" "${@:2}" CROSS=win32 TOOLCHAIN=x86_64-w64-mingw32-
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows 2000+ i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine '
    OUTPUT="${BASE}_win9x_i686"
    buildrel "${1}" "Windows 95/98 i686" "${@:2}" CROSS=win32 M32=y USESDL1=y NOMT=y TOOLCHAIN='wine '

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

    ARCHIVER=_zip_u

    FILES=(psrc-server.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine '
}
build_editor() {
    BASE="psrc_editor"

    ARCHIVER=_tar_u

    FILES=(psrc-editor)
    OUTPUT="${BASE}_${FPLATDESC}"
    MAKE=debmake buildrel "${1}" "${PLATDESC}" "${@:2}"

    ARCHIVER=_zip_u

    FILES=(psrc-editor.exe)
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y TOOLCHAIN='wine '
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
    OUTPUT="${BASE}_${FPLATDESC}"
    MAKE=debmake buildrel "${1}" "${PLATDESC}" -C tools "${@:2}"

    ARCHIVER=_zip

    FILES=(
        tools/crc/crc.exe
        tools/paftool/paftool.exe
        tools/pkdtool/pkdtool.exe
        tools/platinum/platinum.exe
        tools/ptftool/ptftool.exe
    )
    OUTPUT="${BASE}_windows_i686"
    buildrel "${1}" "Windows i686" -C tools "${@:2}" OS=Windows_NT CC=gcc CFLAGS+=-m32 TOOLCHAIN='wine ' null=NUL
}

for i in "${modules[@]}"; do
    "build_${i}" "${i}" MODULE="${i}" deps= || _exit
done

}
