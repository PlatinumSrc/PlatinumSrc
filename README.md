# PlatinumSrc
A WIP 3D game engine inspired by GoldSrc and Quake<br>

---
**Dependencies**<br>
- Building
    - Linux, Windows, FreeBSD
        - SDL2
        - zlib
    - Linux, FreeBSD
        - A C compiler
        - GNU Make
    - Windows
        - MinGW and Make for Windows, or MSYS2 with a C compiler and GNU Make
    - Xbox
        - NXDK 
            - [The modified CXBE from PR #655 is needed](https://github.com/PQCraft/nxdk/tree/master/tools/cxbe)
            - [The extract-xiso symlink fixes are recommended](https://github.com/PQCraft/extract-xiso)
        - pbgl
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
        - `target` - Build an executable or ROM
        - `run` - Build an executable or ROM and run it
        - `clean` - Clean up files from build
    - Makefile variables
        - `MODULE` - Which module to build \(default is `engine`\)
            - `engine`
            - `server`
            - `editor`
            - `toolbox`
        - `CROSS` - Cross compile
            - `freebsd`
            - `win32`
            - `xbox`
        - `EMULATOR` - Set to override the command used to run the ROM 
        - `USE_DISCORD_GAME_SDK` - Set to any value to use the Discord Game SDK
        - `DEBUG` - Enable debug symbols and messages
            - `0` - Symbols only
            - `1` - Basic messages
            - `2` - Advanced messages
            - `2` - Detailed messages
        - `ASAN` - Set to any value to enable the address sanitizer \(requires `DEBUG` to be set\)

    Example command:
    ```
    make DEBUG=1 ASAN=y -j$(nproc) run
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
**Progress \(checked = done, <- = being worked on, ? = maybe\):**<br>
- [ ] Engine
    - [ ] Renderer
        - [ ] OpenGL 1.1
            - [ ] Maps
            - [ ] Entities
            - [ ] Particles
        - [ ] OpenGL 3.3 and ES 3.0
            - [ ] Maps
            - [ ] Entities
            - [ ] Particles
        - [ ] Direct3D 7?
            - [ ] Maps
            - [ ] Entities
            - [ ] Particles
    - [ ] Sound manager
        - Sources \(emulated by scripting\)
            - 0 - `global`: Stereo
            - 1 - `world`: Mono + position effect
            - 2 - `ambient`: Stereo + 2 second crossfade
            - 3 - `music`: Stereo + 2.5 second fade-in and fade-out
            - 4 - `ui`: 1 sound, stereo
- [ ] Server
- [ ] Editor
- [ ] Toolbox
- [ ] File formats
    - [ ] Compiled maps <-
        - [ ] Read PMF
        - [ ] Write PMF
    - [ ] Map projects
        - [ ] Read PMP
        - [ ] Write PMP
    - [ ] 3D models
        - [ ] Read P3M
        - [ ] Write P3M
