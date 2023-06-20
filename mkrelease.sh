#!/bin/bash

{

source util.sh

tsk "Getting info..."
VER_MAJOR="$(grep '#define VER_MAJOR ' src/version.h | sed 's/#define .* //')"
VER_MINOR="$(grep '#define VER_MINOR ' src/version.h | sed 's/#define .* //')"
VER_PATCH="$(grep '#define VER_PATCH ' src/version.h | sed 's/#define .* //')"
VER="${VER_MAJOR}.${VER_MINOR}.${VER_PATCH}"
printf "${I} ${TB}Version:${TR} [%s]\n" "${VER}"
getchanges() {
    sed -n '/^### DONE:$/,$p' TODO.md | tail -n +2
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
./build.sh || _exit
pause

tsk "Pushing..."
git add . || _exit
git commit -S -m "${VER}" -m "${CNGTEXT}" || _exit
git push || _exit

tsk "Making release..."
git tag -s "${VER}" -m "${CNGTEXT}" || _exit
git push --tags || _exit
gh release create "${VER}" --title "${VER}" --notes "${RELTEXT}" PlatinumSrc*.tar.gz PlatinumSrc*.zip || _exit

tsk "Cleaning up..."
rm -rf PlatinumSrc*.tar.gz PlatinumSrc*.zip

tsk "Done"
exit

fi

}
