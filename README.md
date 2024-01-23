# PlatinumSrc<img src="https://raw.githubusercontent.com/PQCraft/PlatinumSrc/master/icons/engine.png" align="right" height="120"/>
**A WIP retro 3D game engine inspired by GoldSrc and Quake**<br>
Progress can be found [here](TODO.md)

---
- [Building](#Building)
- [Running](#Running)
- [Platforms](#Platforms)

---
#### Demo video
Using [H-74](https://github.com/PQCraft/H-74), and [test_model.p3m](https://github.com/PQCraft/PQCraft/raw/master/test_model.p3m) in `games/test/`

https://github.com/PQCraft/PQCraft/assets/58464017/c68cb59c-4f7d-460d-b744-8eee5044fe3e

---
#### Building
- Dependencies
    - Compiling natively on Unix-like systems
        - GNU Make
        - GCC with GNU Binutils or Clang with LLVM
            - Pass `PREFIX=llvm- CC=clang` to the Makefile to use Clang
            - On 32-bit HaikuOS, pass `CC=gcc-x86` to the Makefile to use the correct GCC executable
        - SDL2
    - Cross-compiling for FreeBSD
        - GNU Make
        - Clang
        - FreeBSD's base.txz extracted to <code>external/FreeBSD_<i>\<version\></i>_<i>\<arch\></i></code>
            - The default version is `12.4`, and the architecture is `x86_64` without `M32=y` and `i686` with `M32=y`
        - FreeBSD SDL2 installed into the extracted base.txz
    - Compiling for Windows
        - Compiling on Windows with MSYS2
            - MSYS2
            - GNU Make
            - GCC with GNU Binutils or Clang with LLVM
                - Pass `PREFIX=llvm- CC=clang` to the Makefile to use Clang
            - MinGW SDL2
        - Compiling on Windows without MSYS2
            - Git bash
            - Make for Windows
            - MinGW
            - MinGW SDL2
        - Cross-compiling on Unix-like platforms
            - GNU Make
            - MinGW
            - MinGW SDL2
    - Compiling for Windows 2000 or Windows 98 with KernelEx
        - Cross-compiling on Unix-like platforms
            - Wine
        - Cross-compiling on Unix-like platforms and compiling on Windows
            - [MinGW 7.1.0 win32 sjlj](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/7.1.0/threads-win32/sjlj/i686-7.1.0-release-win32-sjlj-rt_v5-rev2.7z/download)
                - It might work with other versions but they need to not require `___mb_cur_max_func` from msvcrt.dll or `AddVectoredExceptionHandler` from kernel32.dll
            - [psrc-sdl2 MinGW 7.1.0 build](https://github.com/PQCraft/psrc-sdl2/releases/latest/download/SDL2-devel-2.29.0-mingw-7.1.0.zip)
    - Compiling for web browsers using Emscripten
        - GNU Make
        - Emscripten
    - Compiling for the Xbox using the NXDK
        - [NXDK](https://github.com/XboxDev/nxdk)
            - [The modified CXBE from PR #655 is needed](https://github.com/PQCraft/nxdk/tree/master/tools/cxbe)
            - [The extract-xiso symlink fixes are recommended](https://github.com/PQCraft/extract-xiso)
            - [See here for NXDK's dependencies](https://github.com/XboxDev/nxdk/wiki/Install-the-Prerequisites)
        - [pbGL](https://github.com/fgsfdsfgs/pbgl)
            1. Go to the NXDK directory
            2. Copy the pbgl folder into `lib/`
            3. Add `include $(NXDK_DIR)/lib/pbgl/Makefile` to `lib/Makefile`

- Setup
    - Xbox
        1. Create a directory called `xiso`
        2. Copy \(or symlink\) the `common` and `engine` directories into `xiso/`
        3. Also copy \(or symlink\) the games and/or mods you want to include in the disc image
            - There should be a directory \(or link\) called `games` and if you have mods, a directory \(or link\) called `mods`
    - Windows 2000 or Windows 98 with KernelEx
        1. Download [MinGW 7.1.0 win32 sjlj](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/7.1.0/threads-win32/sjlj/i686-7.1.0-release-win32-sjlj-rt_v5-rev2.7z/download)
            - On Linux, use Wine and add MinGW's bin folder to the `PATH` \(can be done using regedit to modify `HKEY_LOCAL_MACHINE\System\CurrentControlSet\Control\Session Manager\Environment\PATH`\)
            - On Windows, add MinGW's bin folder to the `PATH` \(can be done using the environment variable editor\)
        2. Download the [MinGW 7.1.0 build of psrc-sdl2](https://github.com/PQCraft/psrc-sdl2/releases/latest/download/SDL2-devel-2.29.0-mingw-7.1.0.zip), and extract it to `external/Windows_i686`
        3. When using the Makefile, ensure that it uses the correct tools
            - On Linux, pass `M32=y PREFIX='wine ' CC='wine i686-w64-mingw32-gcc' inc.null=NUL` to the Makefile
            - On Windows, pass `M32=y PREFIX= CC=i686-w64-mingw32-gcc` to the Makefile

- Using the Makefile
    - Makefile rules
        - `build` - Build an executable or ROM
        - `run` - Build an executable or ROM and run it
        - `clean` - Clean up intermediate files
        - `distclean` - Clean up intermediate and output files
        - `externclean` - Clean up external tools
    - Makefile variables
        - `MODULE` - Which module to build \(default is `engine`\)
            - `engine` - Game engine
            - `server` - Standalone server
            - `editor` - Map editor
        - `CROSS` - Cross compile
            - `freebsd` - FreeBSD
            - `win32` - Windows 2000+ or Windows 98 with KernelEx
            - `emscr` - Emscripten
            - `nxdk` - Xbox using NXDK
        - `RUNFLAGS` - Flags to pass to the executable
        - `EMULATOR` - Command used to run the executable or ROM
        - `EMUFLAGS` - Flags to pass to the emulator
        - `EMUPATHFLAG` - Flag used to specify the executable or ROM path
        - `O` - Set the optimization level \(defaults to 2\)
        - `M32` - Produce a 32-bit binary
        - `NATIVE` - Tune build for native system
        - `DEBUG` - Enable debug symbols and messages
            - `0` - Symbols only
            - `1` - Basic messages
            - `2` - Advanced messages
            - `3` - Detailed messages
        - `ASAN` - Set to any value to enable the address sanitizer \(requires `DEBUG` to be set\)
        - `NOSTRIP` - Do not strip symbols
        - `NOLTO` - Disable link-time optimization
        - `NOFASTMATH` - Disable `-ffast-math`
        - `CC` - C compiler
        - `LD` - Linker \(defaults to `CC`'s value\)
        - `AR` - Archiver
        - `STRIP` - Symbol remover
        - `OBJCOPY` - Executable editor
        - `WINDRES` - Windows resource compiler
        - `PREFIX` - Text to prepend to tool names
        - `CFLAGS` - Extra C compiler flags
        - `CPPFLAGS` - Extra C preprocessor flags
        - `LDFLAGS` - Extra linker flags
        - `USE_DISCORD_GAME_SDK` - Use the Discord Game SDK
        - `WINPTHREAD` - Use winpthread instead of win32 threads on Windows
        - `NOSIMD` - Do not use SIMD
        - `NOMT` - Disable multithreading
        - `NOGL` - Disable all OpenGL
        - `NOGL11` - Disable OpenGL 1.1
        - `NOGL33` - Disable OpenGL 3.3
        - `NOGLES30` - Disable OpenGL ES 3.0
        - `FREEBSD_VERSION` - \(FreeBSD\) Version to cross-compile to \(default is `12.4`\)
        - `CXBE` - \(NXDK\) Path to cxbe
        - `EXTRACT_XISO` - \(NXDK\) Path to extract-xiso
        - `XBE_TITLE` - \(NXDK\) XBE and XISO name \(default is `PlatinumSrc`\)
        - `XBE_TITLEID` - \(NXDK\) XBE title ID \(default is `PQ-001`\)
        - `XBE_VERSION` - \(NXDK\) XBE version \(default is taken from `version.h`\)
        - `XBE_XTIMAGE` - \(NXDK\) Path to XPR image \(default is `icons/engine.xpr`\)

    Examples:
    ```
    make -j$(nproc)
    ```
    ```
    make -j$(nproc) run
    ```
    ```
    make DEBUG=1 ASAN=y -j$(nproc) run
    ```
    ```
    make CROSS=nxdk DEBUG=0 -j$(nproc) run
    ```

---
#### Running
- Dependencies
    - Running the engine or editor on Unix-like platforms
        - SDL2

- Running the engine
    1. Download a game \(the engine will not run without a game\)
        - [H-74](https://github.com/PQCraft/H-74)
    2. Drop the game into a directory called `games` and ensure the `defaultgame` variable in `engine/config/config.cfg` is set to the game's directory name
    3. Put any mods into a directory called `mods` and ensure they are listed in the `mods` variable in one of the configs
        - You can use `config/config.cfg` in `engine/` or in the game's local data directory \(usually located in `~/.local/share/` under Linux and other Unix-like systems, and `%AppData%\Roaming\` under Windows\)
        - Mods are listed as comma-separated values without spaces between values
    4. Run the executable

---
#### Platforms
- Supported
    - Linux
    - Windows 2000+ and Windows 98 with KernelEx
    - MacOS
    - HaikuOS
    - Emscripten
    - Xbox \(NXDK\)
- Untested
    - FreeBSD
    - NetBSD
    - OpenBSD
- Wanted
    - Android
    - Xbox \(XDK\)?
        - No OpenGL
        - Uses MSVC
    - Dreamcast?
        - SDL 1.2 instead of 2.x
        - RAM might be an issue
    - PS2
    - PSP
    - PS Vita
    - 3DS
    - GameCube?
        - SDL 1.2 instead of 2.x
        - OpenGX is not OpenGL and gl2gx is unmaintained
    - Wii?
        - SDL 1.2 instead of 2.x
        - OpenGX is not OpenGL and gl2gx is unmaintained
    - Windows 95/98 without KernelEx?
        - No SDL
    - MSDOS?
        - Requires low-level tomfoolery
