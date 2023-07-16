#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
rm -rf PlatinumSrc*.tar.gz PlatinumSrc*.zip

# NOTE: Remove the extra stuff on the Windows i686 lines. That's some trolling to get it to run on XP.

build() {
    pkgrel() { _tar_u "PlatinumSrc_Client_${PLATPATH}" psrc; }
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    pkgrel() { _tar_u "PlatinumSrc_Client_${PLATPATH32}" psrc; }
    buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
    #pkgrel() { _zip_u "PlatinumSrc_Client_Windows_x86_64" psrc.exe; }
    #buildrel "${1}" "Windows x86_64" "${@:2}" CROSS=win32
    pkgrel() { _zip_u "PlatinumSrc_Client_Windows_i686" psrc.exe; }
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' WINDRES='wine i686-w64-mingw32-windres' null=NUL
}
buildmod "engine"

build() {
    pkgrel() { _tar_u "PlatinumSrc_Client_${PLATPATH}" psrc-server; }
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    pkgrel() { _tar_u "PlatinumSrc_Client_${PLATPATH32}" psrc-server; }
    buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
    #pkgrel() { _zip_u "PlatinumSrc_Client_Windows_x86_64" psrc-server.exe; }
    #buildrel "${1}" "Windows x86_64" "${@:2}" CROSS=win32
    pkgrel() { _zip_u "PlatinumSrc_Client_Windows_i686" psrc-server.exe; }
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' WINDRES='wine i686-w64-mingw32-windres' null=NUL
}
buildmod "server"

build() {
    pkgrel() { _tar_u "PlatinumSrc_DevTools_${PLATPATH}" psrc-editor; }
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    #pkgrel() { _tar_u "PlatinumSrc_DevTools_${PLATPATH32}" psrc-editor; }
    #buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
    #pkgrel() { _zip_u "PlatinumSrc_DevTools_Windows_x86_64" psrc-editor.exe; }
    #buildrel "${1}" "Windows x86_64" "${@:2}" CROSS=win32
    pkgrel() { _zip_u "PlatinumSrc_DevTools_Windows_i686" psrc-editor.exe; }
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' WINDRES='wine i686-w64-mingw32-windres' null=NUL
}
buildmod "editor"

build() {
    pkgrel() { _tar_u "PlatinumSrc_DevTools_${PLATPATH}" psrc-toolbox; }
    buildrel "${1}" "${PLATDESC}" "${@:2}"
    #pkgrel() { _tar_u "PlatinumSrc_DevTools_${PLATPATH32}" psrc-toolbox; }
    #buildrel "${1}" "${PLATDESC32}" "${@:2}" M32=y
    #pkgrel() { _zip_u "PlatinumSrc_DevTools_Windows_x86_64" psrc-toolbox.exe; }
    #buildrel "${1}" "Windows x86_64" "${@:2}" CROSS=win32
    pkgrel() { _zip_u "PlatinumSrc_DevTools_Windows_i686" psrc-toolbox.exe; }
    buildrel "${1}" "Windows i686" "${@:2}" CROSS=win32 M32=y CC='wine i686-w64-mingw32-gcc' WINDRES='wine i686-w64-mingw32-windres' null=NUL
}
buildmod "toolbox"

}
