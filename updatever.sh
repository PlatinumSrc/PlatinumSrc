#!/bin/bash

{

VERSION_H="src/psrc/version.h"
DATE="$(date -u +'%Y%m%d')"

VERLINE="$(grep -E '\s*#define\s+PSRC_BUILD\s+' "${VERSION_H}")"
VERDEF="$(printf '%s' "${VERLINE}" | grep -Eo '\s*#define\s+PSRC_BUILD\s+')"
CURVER="$(printf '%s' "${VERLINE}" | sed -E 's/\s*#define\s+PSRC_BUILD\s+//')"

VERDATE="${CURVER::${#CURVER}-2}"
VERREV="${CURVER:${#CURVER}-2}"

if [ "${VERDATE}" -lt "${DATE}" ]; then
    NEWVER="${DATE}00"
else
    if [ "${VERREV}" -ge 99 ]; then
        echo "Reached max revision for today (99)" 1>&2
        exit 1
    fi
    NEWVER="${DATE}$(printf '%02u' "$((10#${VERREV}+1))")"
fi

sed -Ei 's/\s*#define\s+PSRC_BUILD\s+.*/'"${VERDEF}${NEWVER}"'/' "${VERSION_H}"

echo "Updated to ${NEWVER}"

}
