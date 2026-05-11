if [ -z "${UTIL_SH+x}" ]; then

UTIL_SH=''

I=$'\e[0m\e[1m\e[37m[\e[36mi\e[37m]\e[0m'
E=$'\e[0m\e[1m\e[37m[\e[31mX\e[37m]\e[0m'
Q=$'\001\e[0m\e[1m\e[37m\002[\001\e[32m\002?\001\e[37m\002]\001\e[0m\002'
H=$'\e[0m\e[1m\e[37m[\e[34m-\e[37m]\e[0m'
T=$'\e[0m\e[1m\e[33m>>>\e[0m'
TB=$'\e[0m\e[1m\e[37m'
TR=$'\e[0m'
TBQ=$'\001\e[1m\e[37m\002'
TRQ=$'\001\e[0m\002'

inf() { printf "%b %b%s%b%s\n" "$I" "$TB" "$1" "$TR" "${2-}"; }
err() { printf "%b %b%s%b%s\n" "$E" "$TB" "$1" "$TR" "${2-}"; }
qry() { printf "%b %b%s%b%s\n" "$Q" "$TB" "$1" "$TR" "${2-}"; }
tsk() { printf "%b %b%s%b%s\n" "$T" "$TB" "$1" "$TR" "${2-}"; }

PLATNAME="$(uname -s)"
PLATARCH="$(uname -m)"
PLATDESC="${PLATNAME} ${PLATARCH}"

# FIX: i386 uname is invalid; use linux subshell or proper arch tool
PLATNAME32="$(linux32 uname -s 2>/dev/null || true)"
PLATARCH32="$(linux32 uname -m 2>/dev/null || true)"
PLATDESC32="${PLATNAME32} ${PLATARCH32}"

ask() {
    read -e -p "${Q} ${TBQ}${1}${TRQ} ${TBQ}>${TRQ} " -i "${3}" "${2}"
}

ask_multiline() {
    read -e -p "${Q} ${TBQ}${1}${TRQ} (Ctrl+D to finish) ${TBQ}>${TRQ} " -i "${3}" "${2}" || true

    local TMP
    while IFS= read -r TMP; do
        eval "${2}=\"\${${2}}\"\$'\n'\"\${TMP}\""
    done
}

pause() {
    printf "%b Press enter to continue..." "$H $TB"
    read -r
    echo
}

_exit() {
    local ERR=${1:-$?}
    err "Error ${ERR}"
    exit "${ERR}"
}

_tar() {
    rm -f -- "${1}.tar.gz"
    tar --transform='s|.*/||' -c -f - -- "${@:2}" | gzip -9 > "${1}.tar.gz"
}

_zip() {
    rm -f -- "${1}.zip"
    zip -qjr9 "./${1}.zip" -- "${@:2}"
}

_tar_u() {
    if [[ -f "${1}.tar.gz" ]]; then
        gzip -d "${1}.tar.gz"
        tar --transform='s|.*/||' -r -f "${1}.tar" "${@:2}" >/dev/null
        gzip -9 "${1}.tar"
    else
        _tar "${@}"
    fi
}

_zip_u() {
    zip -uqjr9 "./${1}.zip" -- "${@:2}"
}

_tar_r() {
    rm -f -- "${1}.tar.gz"
    tar -c -f - -- "${@:2}" | gzip -9 > "${1}.tar.gz"
}

_zip_r() {
    rm -f -- "${1}.zip"
    zip -qr9 "./${1}.zip" -- "${@:2}"
}

fi
