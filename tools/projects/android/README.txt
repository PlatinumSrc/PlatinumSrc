Setup:

     1. Install the Android SDK, NDK, and various tools.
     2. Clone, move, or link SDL to 'app/jni/SDL'.
     3. Clone, move, or link PlatinumSrc to 'app/jni/psrc'.
     4. Update the path of the 'psrc.java' file. It is currently in 'org/psrc/psrc/'. Change it to something like 'org/
       myorg/mygame/' or 'com/mystudio/mygame/'. Keep the SDL files in 'org/libsdl/app/'.
     5. Update the namespace in 'app/build.gradle' from 'org.psrc.psrc' to match the new path ('org.myorg.mygame' for
       example).
     6. Update the app name 'app/src/main/res/values/strings.xml' and the icons in 'app/src/main/res/mipmap-*'.
     7. Use the PAF tool to create an archive with the 'internal' dir only including the 'engine' and 'server' subdirs,
        and a 'games' dir with your game folder in it.
        Example:
            internal/
                engine/
                    ...
                server/
                    ...
            games/
                mygame/
                    ...
     8. Move the PAF file to 'app/src/main/assets/'.
     9. In 'app/jni/CMakeLists.txt', set 'PSRC_ROMFS' to the name you gave the PAF file.

Building:

    - Ensure 'ANDROID_HOME' is set, and run './gradlew build'.
    - Rebuilding appears to not work correctly unless you remove the APKs using something like
      'rm -r app/build/outputs/apk/*'.


