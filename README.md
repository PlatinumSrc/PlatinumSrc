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
        - SDL 2.x or 1.2.x
    - Compiling for Windows
        - Compiling on Windows with MSYS2
            - MSYS2
            - GNU Make
            - GCC with GNU Binutils or Clang with LLVM
                - Pass `PREFIX=llvm- CC=clang` to the Makefile to use Clang
            - MinGW SDL 2.x or 1.2.x
        - Compiling on Windows without MSYS2
            - Git bash
            - Make for Windows
            - MinGW
            - MinGW SDL 2.x or 1.2.x
        - Cross-compiling on Unix-like platforms
            - GNU Make
            - MinGW
            - MinGW SDL 2.x or 1.2.x
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
    <!--
    - Compiling for the PlayStation 2 using the ps2dev sdk
        - [ps2dev](https://github.com/ps2dev/ps2dev)
            - See [this forum post](https://www.ps2-home.com/forum/viewtopic.php?t=9488) for a tutorial
    -->
    - Compiling for the Dreamcast using KallistiOS
        - [KallistiOS](http://gamedev.allusion.net/softprj/kos)
            - See [this wiki page](https://dreamcast.wiki/Getting_Started_with_Dreamcast_development) for a tutorial
        - [img4dc](https://github.com/Kazade/img4dc)
            1. Go into the KallistiOS directory
            2. Go into `utils/`
            3. Git clone `https://github.com/Kazade/img4dc`
            4. Enter `img4dc/` and build it

- Setup
    - Xbox using the NXDK
        1. Create a directory called `xiso`
        2. Copy \(or symlink\) the `engine` directory into `xiso/`
        3. Copy \(or symlink\) the games and/or mods you want to include in the disc image
            - There should be a directory \(or link\) called `games`, and if you have mods, a directory \(or link\) called `mods`
    <!--
    - PS2 using the ps2dev sdk
    -->
    - Dreamcast using KallistiOS
        1. Create a directory called `cdi`
        2. Copy \(or symlink\) the `engine` directory into `cdi/`
        3. Copy \(or symlink\) the games and/or mods you want to include in the disc image
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
        - Build options
            - `MODULE` - Which module to build \(default is `engine`\)
                - `engine` - Game engine
                - `server` - Standalone server
                - `editor` - Map editor
            - `CROSS` - Cross compile
                - `win32` - Windows 2000+ or Windows 98 with KernelEx
                - `emscr` - Emscripten
                - `nxdk` - Xbox using NXDK
                <!--
                - `ps2` - PS2 using ps2dev sdk
                -->
                - `dc` - Dreamcast using KallistiOS
            - `O` - Set the optimization level \(default is `2` if `DEBUG` is unset or `g` if `DEBUG` is set\)
            - `M32` - Set to `y` to produce a 32-bit binary
            - `NATIVE` - Set to `y` to tune build for native system
            - `DEBUG` - Enable debug symbols and messages
                - `0` - Symbols only
                - `1` - Basic messages
                - `2` - Advanced messages
                - `3` - Detailed messages
            - `ASAN` - Set to `y` to enable the address sanitizer \(requires `DEBUG` to be set\)
            - `NOSTRIP` - Set to `y` to not strip symbols
            - `NOLTO` - Set to `y` to disable link-time optimization \(ignored if `DEBUG` is set\)
            - `NOFASTMATH` - Set to `y` to disable `-ffast-math`
            - `NOSIMD` - Set to `y` to not use SIMD
            - `NOMT` - Set to `y` to disable multithreading
        - Features
            - `USEDISCORDGAMESDK` - Set to `y` to use the Discord Game SDK
            - `USEGL` - Set to `y` to include OpenGL support
            - `USEGL11` - Set to `y` to include OpenGL 1.1 support
            - `USEGL33` - Set to `y` to include OpenGL 3.3 support
            - `USEGLES30` - Set to `y` to include OpenGL ES 3.0 support
            - `USEGLAD` - Set to `y` to use glad instead of the system's GL library directly
            - `USEWEAKGL` - Set to `y` to mark `gl[A-Z]*` symbols as weak
            - `USEMINIMP3` - Set to `y` to include MiniMP3 for MP3 support
            - `USESTDTHREAD` - Set to `y` to use C11 threads
            - Windows
                - `USEWINPTHREAD` - Set to `y` to use winpthread instead of win32 threads
        - Toolchain
            - `CC` - C compiler
            - `LD` - Linker \(defaults to `CC`'s value\)
            - `AR` - Archiver
            - `STRIP` - Symbol remover
            - `OBJCOPY` - Executable editor
            - `PREFIX` - Text to prepend to tool names
            - `CFLAGS` - Extra C compiler flags
            - `CPPFLAGS` - Extra C preprocessor flags
            - `LDFLAGS` - Extra linker flags
            - `LDLIBS` - Extra linker libraries
            - `RUNFLAGS` - Flags to pass to the executable
            - `EMULATOR` - Command used to run the executable or ROM
            - `EMUFLAGS` - Flags to pass to the emulator
            - `EMUPATHFLAG` - Flag used to specify the executable or ROM path
            - Windows
                - `WINDRES` - Windows resource compiler
            - NXDK
                - `XBE_TITLE` - XBE title and XISO name \(default is `PlatinumSrc`\)
                - `XBE_TITLEID` - XBE title ID \(default is `PQ-001`\)
                - `XBE_VERSION` - XBE version \(default is taken from `version.h`\)
                - `XBE_XTIMAGE` - Path to XPR image \(default is `icons/engine.xpr`\)
                - `XISO` - Path to write XISO to \(default is `$(OUTDIR)/$(XBE_TITLE).xiso.iso`\)
                - `XISODIR` - Path to make the XISO from \(default is `$(OUTDIR)/xiso`\)
            - Dreamcast
                - `IP_TITLE` - IP.BIN title and CDI name \(default is `PlatinumSrc`\)
                - `IP_COMPANY` - IP.BIN company name \(default is `PQCraft`\)
                - `IP_MRIMAGE` - Path to MR image \(default is `icons/engine.mr`\)
                - `CDI` - Path to write CDI to \(default is `$(OUTDIR)/$(IP_TITLE).cdi`\)
                - `CDIDIR` - Path to make the CDI from \(default is `$(OUTDIR)/cdi`\)

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
        - SDL 2.x or 1.2.x

- Running the engine
    1. Download a game \(the engine will not run without a game\)
        - [H-74](https://github.com/PQCraft/H-74)
    2. Drop the game into a directory called `games` and use the `-game` option, or ensure the `defaultgame` variable in `engine/config/config.cfg` is set to the game's directory name
    3. Put any mods into a directory called `mods` and use the `-mods` option, or ensure they are listed in the `mods` variable in one of the configs
        - You can use `config/config.cfg` in `engine/` or in the game's local data directory \(usually located in `~/.local/share/` under Linux and other Unix-like systems, and `%AppData%\Roaming\` under Windows\)
        - Mods are listed as comma-separated values without spaces between values
    4. Run the executable

---
#### Platforms
- Supported
    - Linux
    - Windows 2000+
    - Windows 98 with KernelEx
    - MacOS
    - HaikuOS
    - Emscripten
    - Xbox \(NXDK\)
    - Dreamcast
- Untested
    - FreeBSD
    - NetBSD
    - OpenBSD
    - Windows 9x no KernelEx
- In progress
    - 3DS
    - GameCube
        - Needs OpenGX renderer
    - Wii
        - Needs OpenGX renderer
- Wanted
    - Android
        - Need to finish the touch UI
        - Need to figure out how to build directly from the Makefile
    - UWP?
        - Requires D3D or can use ANGLE for OpenGL ES 3.0
    - Xbox \(XDK\)?
        - No OpenGL
        - Uses MSVC
    - PS2
        - ps2gl is not OpenGL
    - PSP
    - PS Vita
    - MSDOS?
        - Requires low-level tomfoolery
