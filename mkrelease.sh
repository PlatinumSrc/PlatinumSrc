#!/bin/bash

{

source util.sh

tsk 'Getting info...'
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

tsk 'Building...'
./buildrelease.sh || _exit
inf 'Making psrc_docs.zip...'
_zip_r 'psrc_docs' docs/ || _exit
inf 'Making psrc_files_engine.zip...'
_zip_r 'psrc_files_engine' internal/engine/ internal/server/ || _exit
inf 'Making psrc_files_server.zip...'
_zip_r 'psrc_files_server' internal/server/ || _exit
inf 'Making psrc_files_editor.zip...'
_zip_r 'psrc_files_editor' internal/editor/ internal/engine/ internal/server/ || _exit
cd tools
inf 'Making psrc_tools_any.zip...'
_zip_r '../psrc_tools_any' blender/addons/*/*.txt blender/addons/*/*.py gtksourceview/*.lang || _exit
# TODO: make platform-specific tools
cd ..
pause

tsk 'Pushing...'
git add . || _exit
git commit -S -m "${VER}" -m "${CNGTEXT}" || _exit
git push || _exit

tsk 'Making release...'
git tag -s "${VER}" -m "${CNGTEXT}" || _exit
git push --tags || _exit
gh release create "${VER}" --title "Build ${VER}" --notes "${RELTEXT}" psrc_*.tar.gz psrc_*.zip || _exit

tsk 'Cleaning up...'
./cleanrelease.sh

tsk 'Done'
exit

}
