#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
rm -rf PlatinumSrc*.tar.gz PlatinumSrc*.zip

build() {
    # Current platform
    pkgrel() { _tar "PlatinumSrc_$(uname -s)_$(uname -m).tar.gz" psrc; }
    buildrel "${1}" "$(uname -o) $(uname -m)" ${@:2}
    # Current platform 32-bit
    pkgrel() { _tar "PlatinumSrc_$(i386 uname -s)_$(i386 uname -m).tar.gz" psrc; }
    buildrel "${1}" "$(i386 uname -o) $(i386 uname -m)" ${@:2}
    # Windows x86_64
    pkgrel() { _zip "PlatinumSrc_Windows_x86_64.zip" psrc; }
    buildrel "${1}" "Windows x86_64" ${@:2}
    # Windows i686
    pkgrel() { _zip "PlatinumSrc_Windows_i686.zip" psrc; }
    buildrel "${1}" "Windows i686" ${@:2}
}
buildmod "game"

}
