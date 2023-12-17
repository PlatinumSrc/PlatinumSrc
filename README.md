# PlatinumSrc
#### A WIP 3D game engine inspired by GoldSrc and Quake<br>
Progress can be found [here](TODO.md)

---
**Dependencies**<br>
- Building
    - All platforms
        - SDL2
        - zlib
    - Unix-like platforms
        - A C compiler
        - GNU Make
    - Windows
        - Git bash with MinGW and Make for Windows in the `PATH`, or MSYS2 with a C compiler and GNU Make
    - Xbox
        - [NXDK](https://github.com/XboxDev/nxdk)
            - [The modified CXBE from PR #655 is needed](https://github.com/PQCraft/nxdk/tree/master/tools/cxbe)
            - [The extract-xiso symlink fixes are recommended](https://github.com/PQCraft/extract-xiso)
        - [pbGL](https://github.com/fgsfdsfgs/pbgl)
            1. Go to the NXDK directory
            2. Copy the pbgl folder into `lib/`
            3. Add `include $(NXDK_DIR)/lib/pbgl/Makefile` to `lib/Makefile`

- Running
    - Linux, FreeBSD
        - SDL2

---
**Building**<br>
- Setup
    - Xbox
        1. Create a directory called `xiso`
        2. Copy \(or symlink\) the `common` and `engine` directories into `xiso/`
        3. Also copy \(or symlink\) the games and/or mods you want to include in the disc image
            - There should be a directory \(or link\) called `games` and if you have mods, a directory \(or link\) called `mods`

- Using the Makefile
    - Makefile rules
        - `build` - Build an executable or ROM
        - `run` - Build an executable or ROM and run it
        - `clean` - Clean up files from build
    - Makefile variables
        - `MODULE` - Which module to build \(default is `engine`\)
            - `engine`
            - `server`
            - `editor`
        - `CROSS` - Cross compile
            - `freebsd`
            - `win32`
            - `nxdk`
        - `FREEBSD_VERSION` - Version of FreeBSD to cross-compile to \(default is `12.4`\)
        - `EMULATOR` - Set to override the command used to run the ROM
        - `USE_DISCORD_GAME_SDK` - Set to any value to use the Discord Game SDK
        - `NATIVE` - Tune build for native system
        - `M32` - Produce a 32-bit binary
        - `NOSTRIP` - Do not strip symbols
        - `NOLTO` - Disable link-time optimization
        - `DEBUG` - Enable debug symbols and messages
            - `0` - Symbols only
            - `1` - Basic messages
            - `2` - Advanced messages
            - `3` - Detailed messages
        - `ASAN` - Set to any value to enable the address sanitizer \(requires `DEBUG` to be set\)

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
**Running**<br>
- Engine
    1. Download a game \(the engine will not run without a game\)
        - The [H-74](https://github.com/PQCraft/H-74) repo contains a few games and mods
    2. Drop the game into a directory called `games` and ensure the `defaultgame` variable in `engine/config/config.cfg` is set to the game's directory name
    3. Put any mods into a directory called `mods` and ensure they are listed in the `mods` variable in one of the configs
        - You can use `config/config.cfg` in `engine/` or in the game's local data directory (usually located in `~/.local/share/` under Linux and other Unix-like systems, and `%AppData%\Roaming\` under Windows)
        - Mods are listed as comma-separated values without spaces between values
    4. Run the executable

---
**Platforms**<br>
- Supported
    - Linux
    - Windows XP+
    - Xbox (NXDK)
- Untested
    - Windows 2000
    - MacOS
    - FreeBSD
    - NetBSD
    - OpenBSD
- Wanted
    - Android
    - Emscripten
    - Xbox (XDK)?
        - No OpenGL
        - Uses MSVC
    - Dreamcast?
        - SDL 1.2 instead of 2.x
        - RAM might be an issue
    - PS2
    - PSP
    - PS Vita
    - GameCube?
        - SDL 1.2 instead of 2.x
        - OpenGX is not OpenGL and gl2gx is unmaintained
    - Wii?
        - SDL 1.2 instead of 2.x
        - OpenGX is not OpenGL and gl2gx is unmaintained
    - Windows 95/98?
        - No SDL
    - MSDOS?
        - Requires low-level tomfoolery
