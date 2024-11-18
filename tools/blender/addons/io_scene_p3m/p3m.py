from math import log2
from array import array
from struct import pack
from sys import byteorder

import bpy
import mathutils

# TODO: Add """docs"""

class p3m:

    PARTS_MAX = 255
    MATERIALS_MAX = 255
    TEXTURES_MAX = 255
    BONES_MAX = 255
    ANIMATIONS_MAX = 255
    ACTIONS_MAX = 255
    STRINGTABLE_MAX = 65535

    class flags:
        pass

    class part:

        VERTICES_MAX = 65535
        INDICES_MAX = 65535

        class flags:
            HASNORMALS = 1 << 0

        def __init__(
            self, name: str, nameind: int, *, flags: int = 0,
            mat: int = 0, verts: list[float] = [], norms: list[float] = [], inds: list[int] = [],
            weights: list[(int, list[(int, array)])] = None
        ):
            self.name = name
            self.nameind = nameind
            self.flags = flags
            self.mat = mat
            self.verts = array('f', verts) # [][5] (x, y, z, u, v)
            self.norms = array('f', norms) # [][3] (x, y, z)
            self.inds = array('H', inds)
            self.weights = weights if weights is not None else []

        def tofile(self, f):
            f.write(pack('<BHB', self.flags, self.nameind, self.mat))
            if byteorder == 'little':
                f.write(pack(f'<H', len(self.verts) // 5))
                self.verts.tofile(f)
                self.norms.tofile(f)
                f.write(pack(f'<H', len(self.inds)))
                self.inds.tofile(f)
                f.write(pack(f'<B', len(self.weights)))
                for l in self.weights:
                    f.write(pack(f'<H', l[0]))
                    for w in l[1]:
                        f.write(pack(f'<H', w[0]))
                        f.write(pack(f'<H', len(w[1])))
                        w[1].tofile(f)
            else:
                f.write(pack(f'<H{len(self.verts)}f', len(self.verts) // 5, *self.verts))
                f.write(pack(f'<{len(self.norms)}f', *self.norms))
                f.write(pack(f'<H{len(self.inds)}H', len(self.inds), *self.inds))
                for l in self.weights:
                    f.write(pack(f'<H', l[0]))
                    for w in l[1]:
                        f.write(pack(f'<H', w[0]))
                        f.write(pack(f'<H{len(w[1])}H', len(w[1]), *w[1]))

    class material:

        class rendmodes:
            NORMAL = 0
            ADD = 1
            _MAX = 1

        def __init__(self, name: str, *, rendmode: int = 0, tex: int = 255, color: list[int] = (255,) * 4, emis: list[int] = (0,) * 3, shading: int = 255):
            self.name = name
            self.rendmode = rendmode # p3m.material.rendmodes.*
            self.tex = tex
            self.color = array('B', color)
            self.emis = array('B', emis)
            self.shading = shading

        def tofile(self, f):
            f.write(pack('<10B', self.rendmode, self.tex, *self.color, *self.emis, self.shading))

    class texture:

        class types:
            EMBEDDED = 0
            EXTERNAL = 1

        class embedded:
            class ptf:
                ver = 0
                class flags:
                    HASALPHA = (1 << 0)
            def __init__(self, *, channels: int, size, data):
                self.flags = p3m.texture.embedded.ptf.flags.HASALPHA if channels == 4 else 0
                self.size = size if not hasattr(size, '__getitem__') else size[0]
                self.data = data
            def tofile(self, f):
                f.write(pack('<I', 5 + len(self.data)))
                f.write(b"PTF")
                f.write(pack("<2B", p3m.texture.embedded.ptf.ver, (self.flags << 4) | (int(log2(self.size)) & 15)))
                f.write(self.data)

        class external:
            def __init__(self, rcpath, rcpathind):
                self.rcpath = rcpath
                self.rcpathind = rcpathind
            def tofile(self, f):
                f.write(pack('<H', self.rcpathind))

        def __init__(self, type: int, data: embedded | external):
            self.type = type
            self.data = data

        def tofile(self, f):
            f.write(pack("<B", self.type))
            self.data.tofile(f)

    class bone:

        def __init__(self, name: str, nameind: int, *, head: mathutils.Vector, tail: mathutils.Vector, childct: int):
            self.name = name
            self.nameind = nameind
            self.head = array('f', (head[0], head[2], head[1]))
            self.tail = array('f', (tail[0], head[2], head[1]))
            self.childct = childct

        def tofile(self, f):
            f.write(pack("<H6fB",
                self.nameind,
                *self.head, *self.tail,
                self.childct
            ))

    class animation:

        class action:
            def __init__(self, ind: int, *, speed: float = 1.0, start: int, end: int):
                self.ind = ind
                self.speed = speed
                self.start = start
                self.end = end
            def tofile(self, f):
                f.write(pack("<BfHH", self.ind, self.speed, self.start, self.end))

        def __init__(self, name: str, nameind: int, *, data: list[action] = None):
            self.name = name
            self.nameind = nameind
            self.data = data if data is not None else []

        def tofile(self, f):
            f.write(pack('<HB', self.nameind, len(self.data)))
            for d in self.data: d.tofile(f)

    class action:

        class partvis:
            DEFAULTWHITE = 0
            DEFAULTBLACK = 1
            WHITE = 2
            BLACK = 3

        class interp:
            NONE = 0
            LINEAR = 1

        class data:
            KEYFRAMES_MAX = 255
            def __init__(
                self, bonename: str, bonenameind: int, *,
                transframeskips: list[int] = [], transinterps: list[int] = [], transdata: list[float] = [],
                rotframeskips: list[int] = [], rotinterps: list[int] = [], rotdata: list[float] = [],
                scaleframeskips: list[int] = [], scaleinterps: list[int] = [], scaledata: list[float] = []
            ):
                self.bonename = bonename
                self.bonenameind = bonenameind
                self.transframeskips = array('B', transframeskips)
                self.transinterps = array('B', transinterps) # p3m.action.interp.*[]
                self.transdata = array('f', transdata) # [][3] (x, y, z)
                self.rotframeskips = array('B', rotframeskips)
                self.rotinterps = array('B', rotinterps) # p3m.action.interp.*[]
                self.rotdata = array('f', rotdata) # [][3] (x, y, z)
                self.scaleframeskips = array('B', scaleframeskips)
                self.scaleinterps = array('B', scaleinterps) # p3m.action.interp.*[]
                self.scaledata = array('f', scaledata) # [][3] (x, y, z)
            def tofile(self, f):
                f.write(pack('<H3B', self.bonenameind, len(self.transdata) // 3, len(self.rotdata) // 3, len(self.scaledata) // 3))
                if byteorder == 'little':
                    self.transframeskips.tofile(f)
                    self.rotframeskips.tofile(f)
                    self.scaleframeskips.tofile(f)
                    self.transinterps.tofile(f)
                    self.rotinterps.tofile(f)
                    self.scaleinterps.tofile(f)
                    self.transdata.tofile(f)
                    self.rotdata.tofile(f)
                    self.scaledata.tofile(f)
                else:
                    f.write(pack(f'<{len(self.transframeskips)}B', *self.transframeskips))
                    f.write(pack(f'<{len(self.transframeskips)}B', *self.rotframeskips))
                    f.write(pack(f'<{len(self.scaleframeskips)}B', *self.scaleframeskips))
                    f.write(pack(f'<{len(self.transinterps)}B', *self.transinterps))
                    f.write(pack(f'<{len(self.transinterps)}B', *self.rotinterps))
                    f.write(pack(f'<{len(self.scaleinterps)}B', *self.scaleinterps))
                    f.write(pack(f'<{len(self.transdata)}f', *self.transdata))
                    f.write(pack(f'<{len(self.transdata)}f', *self.rotdata))
                    f.write(pack(f'<{len(self.scaledata)}f', *self.scaledata))

        def __init__(
            self, name: str, *, frameus: int, partvis: int = 0,
            partnames: list[str] = None, partnameinds: list[int] = [], data: list[data] = None
        ):
            self.name = name
            self.frameus = frameus
            self.partvis = partvis # p3m.action.partvis.*
            self.partnames = partnames if partnames is not None else []
            self.partnameinds = array('H', partnameinds)
            self.data = data if data is not None else []

        def tofile(self, f):
            f.write(pack('<I2B', self.frameus, self.partvis, len(self.partnameinds)))
            if byteorder == 'little':
                self.partnameinds.tofile(f)
            else:
                f.write(pack(f'<{len(self.partnameinds)}H', *self.partnameinds))
            f.write(pack('<B', len(self.data)))
            for d in self.data: d.tofile(f)

    ver = 0

    def __init__(self):
        self.flags = 0
        self.parts = [] # p3m.part[]
        self.vismask = array('B', [0] * 32)
        self.materials = [] # p3m.material[]
        self.textures = [] # p3m.texture[]
        self.bones = [] # p3m.bone[]
        self.animations = [] # p3m.animation[]
        self.actions = [] # p3m.action[]
        self.strtbl = b""

    def tofile(self, f):
        f.write(b"P3M")
        f.write(pack("<3B", p3m.ver, self.flags, len(self.parts)))
        self.vismask[0 : (len(self.parts) + 7) // 8].tofile(f)
        for p in self.parts: p.tofile(f)
        f.write(pack("<B", len(self.materials)))
        for m in self.materials: m.tofile(f)
        f.write(pack("<B", len(self.textures)))
        for t in self.textures: t.tofile(f)
        f.write(pack("<B", len(self.bones)))
        for b in self.bones: b.tofile(f)
        f.write(pack("<B", len(self.animations)))
        for a in self.animations: a.tofile(f)
        f.write(pack("<B", len(self.actions)))
        for a in self.actions: a.tofile(f)
        f.write(self.strtbl if len(self.strtbl) else b"\0")
