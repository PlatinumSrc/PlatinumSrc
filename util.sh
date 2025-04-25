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
inf() { printf "${I} ${TB}%s${TR}%s\n" "${@}"; }
err() { printf "${E} ${TB}%s${TR}%s\n" "${@}"; }
qry() { printf "${Q} ${TB}%s${TR}%s\n" "${@}"; }
tsk() { printf "${T} ${TB}%s${TR}%s\n" "${@}"; }

PLATNAME="$(uname -s)"
PLATARCH="$(uname -m)"
PLATDESC="${PLATNAME} ${PLATARCH}"
PLATNAME32="$(i386 uname -s)"
PLATARCH32="$(i386 uname -m)"
PLATDESC32="${PLATNAME32} ${PLATARCH32}"

ask() {
    read -e -p "${Q} ${TBQ}${1}${TRQ} ${TBQ}>${TRQ} " -i "${3}" -- "${2}"
}
ask_multiline() {
    read -e -p "${Q} ${TBQ}${1}${TRQ} (press Ctrl+D when done) ${TBQ}>${TRQ} " -i "${3}" -- "${2}"
    if [ "${?}" -eq 0 ]; then
        local TMP
        while true; do
            read -e -p '> ' TMP
            [ "${?}" -ne 0 ] && break
            eval "${2}=\"\$${2}\"\$'\n'\"\${TMP}\""
        done
    fi
}
pause() {
    printf "${H} ${TB}Press enter to continue...${TR}"
    read -s
    echo
}
_exit() {
    local ERR="${?}"
    [[ ${#} -eq 0 ]] || local ERR="${1}"
    err "Error ${ERR}"
    exit "${ERR}"
}

_tar() {
    rm -f -- "${1}.tar.gz"
    tar --transform 's/.*\///g' -c -f - -- "${@:2}" | gzip -9 > "${1}.tar.gz"
}
_zip() {
    rm -f -- "${1}.zip"
    zip -qjr9 "./${1}.zip" -- "${@:2}"
}
_tar_u() {
    if [[ -f "${1}" ]]; then
        gzip -d "${1}.tar.gz"
        tar --transform 's/.*\///g' -r -f "${1}.tar" "${@:2}" 1> /dev/null
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
