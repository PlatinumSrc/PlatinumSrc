# PlatinumSrc<img src="https://raw.githubusercontent.com/PQCraft/PlatinumSrc/master/internal/resources/icons/logo.png" align="right" height="120"/>
**A WIP retro 3D game engine inspired by GoldSrc and Quake**<br>
Progress can be found [here](TODO.md)

---
- [Platform Support](#platform-support)
- [How to run](#how-to-run)
- [Building from source](#building-from-source)

---
### Demo video
Using [H-74](https://github.com/PQCraft/H-74), and [test_model.p3m](https://github.com/PQCraft/PQCraft/raw/master/test_model.p3m) in `games/test/`

https://github.com/user-attachments/assets/34b922c1-5fe6-409b-96fd-51a7227429c0

---
### Platform Support
<details open><summary>Supported</summary>

- Linux
- Windows 2000+
- Windows 98
- MacOS
- HaikuOS
- Emscripten
</details>
<details><summary>Untested</summary>

- FreeBSD
- NetBSD
- OpenBSD
</details>
<details><summary>In progress</summary>

- Xbox \(NXDK\)
    - Needs an XGU renderer
- Dreamcast
    - Needs a PowerVR renderer
- 3DS
    - Needs a Citro3D renderer
- GameCube
    - Needs an OpenGX renderer
- Wii
    - Needs an OpenGX renderer
- PS2
    - Needs a GSKit renderer
</details>
<details><summary>Wanted</summary>

- Android
    - Need to finish the touch UI
    - Need to figure out how to build directly from the Makefile
- UWP/GameSDK
    - Needs a D3D 9 renderer
- Xbox \(XDK\)
    - Needs a D3D 7/8 renderer
- PSP
- PS Vita
</details>

---
### How to run
<details><summary>Running the engine</summary>

1. Download a game \(the engine will not run without a game\)
    - [H-74](https://github.com/PQCraft/H-74)
2. Drop the game into a directory called `games` and use the `-game` option, or ensure the `defaultgame` variable in `internal/config.cfg` is set to the game's directory name
3. Put any mods into a directory called `mods` and use the `-mods` option, or ensure they are listed in the `mods` variable in one of the configs
    - You can use `config.cfg` in `internal/` or in the game's user data directory
    - Mods are listed as comma-separated values without spaces between values
4. Run the executable
</details>

---
### Building from source
<details open><summary>Building on Unix-like platforms for that same platform</summary>

- Install GNU Make
- Install GCC with GNU Binutils, or Clang with LLVM
    - Pass `TOOLCHAIN=llvm- CC=clang` to the Makefile to use Clang
    - On 32-bit HaikuOS, pass `CC=gcc-x86` to the Makefile to use the correct GCC executable
- Install SDL 2.x or 1.2.x
- If building the dedicated server, pass `MODULE=server` to the Makefile, or if building the editor, pass `MODULE=editor`
</details>
<details open><summary>Building for Windows</summary>

- If cross-compiling on a Unix-like platform
    - Install GNU Make
    - Install MinGW
    - Install MinGW SDL 2.x or 1.2.x
    - Pass `CROSS=win32` to the Makefile
- If MSYS2 is supported
    - Install MSYS2 and use the MINGW64 backend
    - Install GNU Make
    - Install GCC with GNU Binutils, or Clang with LLVM
        - Pass `TOOLCHAIN=llvm- CC=clang` to the Makefile to use Clang
    - Install MinGW SDL 2.x or 1.2.x
- If MSYS2 is not supported
    - Install Git bash
    - Install [Make for Windows](https://sourceforge.net/projects/gnuwin32/files/make/3.81/make-3.81.exe/download) and add it to the `PATH`
    - Download MinGW and add it to the `PATH`
    - Donwload and extract MinGW SDL 2.x or 1.2.x into MinGW
- If building the dedicated server, pass `MODULE=server` to the Makefile, or if building the editor, pass `MODULE=editor`
</details>
<details><summary>Building for older Windows</summary>

- Download [MinGW 7.1.0 win32 sjlj](https://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win32/Personal%20Builds/mingw-builds/7.1.0/threads-win32/sjlj/i686-7.1.0-release-win32-sjlj-rt_v5-rev2.7z/download) and add it to the `PATH`
    - It might work with other versions but they need to not require `___mb_cur_max_func` from msvcrt.dll or `AddVectoredExceptionHandler` from kernel32.dll
- If cross-compiling on a Unix-like platform
    - Install Wine
    - Pass `CROSS=win32 TOOLCHAIN='wine '` to the Makefile
- If bulding for Windows 2000
    - Download [psrc-sdl2 MinGW 7.1.0 build](https://github.com/PQCraft/psrc-sdl2/releases/latest/download/SDL2-devel-2.29.0-mingw-7.1.0.zip), and extract it to `external/Windows_i686`
- If building for Windows 98
    - Download [SDL 1.2.x modified to be compatible with Windows 98](https://github.com/PQCraft/PQCraft/raw/master/SDL_1_2_Win98.zip), and extract it to `external/Windows_i686`
    - Pass `USESDL1=y NOMT=y` to the Makefile
- If building the dedicated server, pass `MODULE=server` to the Makefile, or if building the editor, pass `MODULE=editor`
</details>
<details><summary>Building for web browsers using Emscripten</summary>

- Install GNU Make
- Install Emscripten
- Pass `CROSS=emscr` to the Makefile
</details>
<details><summary>Building for the Xbox using the NXDK</summary>

- Set up the [NXDK](https://github.com/XboxDev/nxdk)
    - [The modified CXBE from PR #655 is needed](https://github.com/PQCraft/nxdk/tree/master/tools/cxbe)
    - [The extract-xiso symlink fixes are recommended](https://github.com/PQCraft/extract-xiso)
    - [See here for NXDK's dependencies](https://github.com/XboxDev/nxdk/wiki/Install-the-Prerequisites)
- Set up [XGU](https://github.com/dracc/xgu)
    1. Go to the NXDK directory
    2. Go into the `lib/` directory
    3. Clone XGU into an `xgu/` directory
- Set up the `xiso` directory
    1. Create a directory called `xiso`
    2. Copy \(or symlink\) the `internal` directory into `xiso/`
    3. Copy \(or symlink\) the games and/or mods you want to include in the disc image
        - There should be a directory \(or link\) called `games`, and if you have mods, a directory \(or link\) called `mods`
- Pass `CROSS=nxdk` to the Makefile
</details>
<details><summary>Building for the Dreamcast using KallistiOS</summary>

- Set up [KallistiOS](http://gamedev.allusion.net/softprj/kos)
    - See [this wiki page](https://dreamcast.wiki/Getting_Started_with_Dreamcast_development) for a tutorial
- Set up [img4dc](https://github.com/Kazade/img4dc)
    1. Go into the KallistiOS directory
    2. Go into `utils/`
    3. Git clone `https://github.com/Kazade/img4dc`
    4. Enter `img4dc/` and build it
- Set up the `cdi` directory
    1. Create a directory called `cdi`
    2. Copy \(or symlink\) the `internal` directory into `cdi/`
    3. Copy \(or symlink\) the games and/or mods you want to include in the disc image
- Pass `CROSS=dc` to the Makefile
</details>
<!--
<details><summary>Building for the PlayStation 2 using the ps2dev sdk</summary>
- Set up the [ps2dev SDK](https://github.com/ps2dev/ps2dev)
    - See [this forum post](https://www.ps2-home.com/forum/viewtopic.php?t=9488) for a tutorial
- Pass `CROSS=ps2` to the Makefile
</details>
-->

———
<details><summary>Full Makefile usage</summary>

- Rules
    - `build` - Build an executable or ROM
    - `run` - Build an executable or ROM and run it
    - `clean` - Clean up intermediate files
    - `distclean` - Clean up intermediate and output files
    - `externclean` - Clean up external tools
- Variables
    - Build options
        - `MODULE` - Which module to build \(default is `engine`\)
            - `engine` - Game engine
            - `server` - Standalone server
            - `editor` - Map editor
        - `CROSS` - Cross compile
            - `win32` - Windows 2000+ or Windows 98 with KernelEx
            - `emscr` - Emscripten
            - `nxdk` - Xbox using NXDK
            - `dc` - Dreamcast using KallistiOS
            <!--
            - `ps2` - PS2 using ps2dev sdk
            -->
        - `ONLYBIN` - Set to `y` to skip making a disc image.
        - `O` - Set the optimization level \(default is `2` if `DEBUG` is unset or `g` if `DEBUG` is set\)
        - `M32` - Set to `y` to produce a 32-bit binary
        - `NATIVE` - Set to `y` to tune the build for the native system
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
    - Features and backends
        - `USESTDIODS` - Set to `y` to use fopen\(\), fread\(\), and fclose\(\) in place of open\(\), read\(\), and close\(\) in the datastream code
        - `USEDISCORDGAMESDK` - Set to `y` to include the Discord Game SDK
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
    - Toolchain options
        - `CC` - C compiler
        - `LD` - Linker \(defaults to `CC`'s value\)
        - `AR` - Archiver
        - `STRIP` - Symbol remover
        - `OBJCOPY` - Executable editor
        - `TOOLCHAIN` - Text to prepend to tool names
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
        - Emscripten
            - `EMSCR_SHELL` - Path to the shell file
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
</details>

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
