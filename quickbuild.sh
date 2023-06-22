#!/bin/bash

{

source util.sh

NJOBS="$(nproc)"
rm -rf PlatinumSrc*.tar.gz PlatinumSrc*.zip

pkgrel() { :; }

build() {
    buildrel "${1}" "$(uname -o)" ${@:2}
    buildrel "${1}" "Windows" ${@:2} CROSS=win32
}
buildmod "engine"

build() {
    buildrel "${1}" "$(uname -o)" ${@:2}
    buildrel "${1}" "Windows" ${@:2} CROSS=win32
}
buildmod "toolbox"

}
