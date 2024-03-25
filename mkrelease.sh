#!/bin/bash

{

source util.sh

tsk "Getting info..."
VER="$(grep '#define PSRC_BUILD ' src/psrc/version.h | sed 's/#define .* //')"
printf "${I} ${TB}Version:${TR} [%s]\n" "${VER}"
getchanges() {
    sed -n '/^### Done$/,$p' TODO.md | tail -n +2
}
CNGTEXT="$(getchanges)"
getreltext() {
    echo '### Changes'
    echo "${CNGTEXT}"
}
RELTEXT="$(getreltext)"
printf "${I} ${TB}Release text:${TR}\n%s\n${TB}EOF${TR}\n" "${RELTEXT}"
pause

tsk "Building..."
./buildrelease.sh || _exit
inf "Making PlatinumSrc_Assets.zip..."
_zip_r "PlatinumSrc_Assets.zip" engine/ icons/
inf "Making PlatinumSrc_Extras.zip..."
_zip_r "PlatinumSrc_Extras.zip" docs/ tools/
pause

tsk "Pushing..."
git add . || _exit
git commit -S -m "${VER}" -m "${CNGTEXT}" || _exit
git push || _exit

tsk "Making release..."
git tag -s "${VER}" -m "${CNGTEXT}" || _exit
git push --tags || _exit
gh release create "${VER}" --title "Build ${VER}" --notes "${RELTEXT}" PlatinumSrc*.tar.gz PlatinumSrc*.zip || _exit

tsk "Cleaning up..."
rm -rf PlatinumSrc*.tar.gz PlatinumSrc*.zip

tsk "Done"
exit

}
