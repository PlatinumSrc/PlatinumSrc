#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
rm -rf PlatinumSrc*.tar.gz PlatinumSrc*.zip

build() {
    pkgrel() { _tar "PlatinumSrc_Engine_$(uname -s)_$(uname -m).tar.gz" psrc; }
    buildrel "${1}" "$(uname -o) $(uname -m)" ${@:2}
    pkgrel() { _tar "PlatinumSrc_Engine_$(i386 uname -s)_$(i386 uname -m).tar.gz" psrc; }
    buildrel "${1}" "$(i386 uname -o) $(i386 uname -m)" ${@:2} M32=y
    pkgrel() { _zip "PlatinumSrc_Engine_Windows_x86_64.zip" psrc.exe; }
    buildrel "${1}" "Windows x86_64" ${@:2} CROSS=win32
    pkgrel() { _zip "PlatinumSrc_Engine_Windows_i686.zip" psrc.exe; }
    buildrel "${1}" "Windows i686" ${@:2} CROSS=win32 M32=y
}
buildmod "engine"

build() {
    pkgrel() { _tar "PlatinumSrc_Server_$(uname -s)_$(uname -m).tar.gz" psrv; }
    buildrel "${1}" "$(uname -o) $(uname -m)" ${@:2}
    pkgrel() { _tar "PlatinumSrc_Server_$(i386 uname -s)_$(i386 uname -m).tar.gz" psrv; }
    buildrel "${1}" "$(i386 uname -o) $(i386 uname -m)" ${@:2} M32=y
    pkgrel() { _zip "PlatinumSrc_Server_Windows_x86_64.zip" psrv.exe; }
    buildrel "${1}" "Windows x86_64" ${@:2} CROSS=win32
    pkgrel() { _zip "PlatinumSrc_Server_Windows_i686.zip" psrv.exe; }
    buildrel "${1}" "Windows i686" ${@:2} CROSS=win32 M32=y
}
buildmod "server"

}
