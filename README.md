# PlatinumSrc
A WIP 90's style 3D engine<br>

---
**Build dependencies:**<br>
```
libogg
libopus
libopusfile
libvorbis
libvorbisfile
SDL2
```

---
**Progress \(checked = done, <- = being worked on\):**<br>
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
        - [ ] DirectX 7
            - [ ] Maps
            - [ ] Entities
            - [ ] Particles
    - [ ] Sound manager
        - Sources
            - 0 - `global`: Circular buffer of channels, stereo
            - 1 - `world`: Circular buffer of channels, mono, position effect
            - 2 - `ambient`: 1 channel, stereo, 2 second crossfade between sound changes
            - 3 - `music`: 1 channel, stereo, 3 second fade-in and fade-out
            - 4 - `ui`: 1 channel, stereo
- [ ] Server
- [ ] Toolbox
    - [ ]
- [ ] File formats
    - [ ] Compiled maps
        - [ ] Read PMF
        - [ ] Write PMF
        - [ ] Read PMZ
        - [ ] Write PMZ
    - [ ] Map projects
        - [ ] Read PMP
        - [ ] Write PMP
    - [ ] 3D models
        - [ ] Read P3M
        - [ ] Write P3M
