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
inf "Making psrc_docs.zip..."
_zip_r "psrc_docs" docs/
inf "Making psrc_reqired_files.zip..."
_zip_r "psrc_reqired_files" internal/
#inf "Making psrc_reqired_server_files.zip..."
#_zip_r "psrc_reqired_files" internal/scripts/
pause

tsk "Pushing..."
git add . || _exit
git commit -S -m "${VER}" -m "${CNGTEXT}" || _exit
git push || _exit

tsk "Making release..."
git tag -s "${VER}" -m "${CNGTEXT}" || _exit
git push --tags || _exit
gh release create "${VER}" --title "Build ${VER}" --notes "${RELTEXT}" psrc_*.tar.gz psrc_*.zip || _exit

tsk "Cleaning up..."
./cleanrelease.sh

tsk "Done"
exit

}
