import bpy
import bmesh
from dataclasses import dataclass
import struct

def save(operator, context, filepath,
        export_bones, export_anims, use_absolute,
        **kwargs
    ):

    @dataclass
    class p3mvert:
        x: float
        y: float
        z: float
        t: int
        u: float
        v: float
        def __eq__(self, other):
            return (
                self.x == other.x and self.y == other.y and self.z == other.z and
                self.t == other.t and self.u == other.u and self.v == other.v
            )
        def __ne__(self, other):
            return (
                self.x != other.x or self.y != other.y or self.z != other.z or
                self.t != other.t or self.u != other.u or self.v != other.v
            )

    print("Exporting " + filepath + "...")

    if bpy.ops.object.mode_set.poll(): bpy.ops.object.mode_set(mode = 'OBJECT')

    armature = None

    vertices = []
    indices = []
    materials = [] # [index, name]
    bones = [] # [name, deform, head, tail, [index, weight], child count]
    actions = [] # [name, frames, [bone, [frame, translation], [frame, rotation], [frame, scale]]]
    animations = [] # [name, frametime, [action, speed, start, end]]
    strtable = b""

    def addtostrtable(s):
        nonlocal strtable
        b = s.encode("utf-8") + b"\0"
        p = strtable.find(b)
        if p == -1:
            l = len(strtable)
            strtable += b
            return l
        return p
    def addmaterial(mi, md):
        nonlocal materials
        for i, m in enumerate(materials):
            if m[0] == mi: return i
        materials.append([mi, md[mi].material.name])
        return len(materials) - 1
    def addvertex(v):
        nonlocal vertices
        for i, v2 in enumerate(vertices):
            if v2 == v: return i
        vertices.append(v)
        return len(vertices) - 1
    def addfaces(fl, md, uvl):
        nonlocal indices
        for f in fl:
            m = addmaterial(f.material_index, md)
            for l in f.loops:
                i = addvertex(p3mvert(
                    l.vert.co.x, l.vert.co.z, l.vert.co.y,
                    m, l[uvl].uv.x, l[uvl].uv.y
                ))
                indices.append(i)
    def addmesh(obj):
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        if use_absolute:
            bmesh.ops.translate(bm, vec=obj.location, verts=bm.verts[:])
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        addfaces(bm.faces, obj.material_slots, bm.loops.layers.uv.active)
        bm.free()

    if export_bones or export_anims:
        print("Finding an armature...")
        for obj in bpy.data.objects:
            if obj.type == 'ARMATURE':
                print("Found an armature: " + obj.name)
                armature = obj
                break
        if armature is None:
            print("No armature found")

    if armature is None:
        print("Finding meshes...")
        for obj in bpy.data.objects:
            if obj.type != 'MESH': continue
            print("Found a mesh: " + obj.name)
            addmesh(obj)
    else:
        print("Finding meshes...")
        for obj in armature.children:
            if obj.type != 'MESH': continue
            print("Found a mesh: " + obj.name)
            addmesh(obj)
            if export_bones:
                def addbone(b):
                    bones.append([
                        b.name,
                        b.use_deform,
                        [b.head_local.x, b.head_local.z, b.head_local.y],
                        [b.tail_local.x, b.tail_local.z, b.tail_local.y],
                        [],
                        len(b.children)
                    ])
                    for c in b.children:
                        addbone(c)
                for bone in armature.data.bones:
                    if bone.parent is None:
                        addbone(bone)
                for v in obj.data.vertices:
                    for g in v.groups:
                        if g.weight > 0.0:
                            name = obj.vertex_groups[g.group].name
                            for b in bones:
                                if b[1] and b[0] == name:
                                    for i, v2 in enumerate(vertices):
                                        if (v.co.x == v2.x and v.co.z == v2.y and v.co.y == v2.z):
                                            b[4].append([i, g.weight])
                                    break
        if export_anims:
            def getfclist(act, name, count):
                fclist = []
                tmp = 0
                for i in range(0, count):
                    fc = act.fcurves.find(name, index=i)
                    fclist.append(fc)
                    if fc is not None: tmp += 1
                if tmp > 0: return fclist
                return None
            def gettimes(fclist):
                times = []
                def addtime(t):
                    nonlocal times
                    for t2 in times:
                        if t2 == t: return
                    times.append(t)
                for fc in fclist:
                    if fc is None: continue
                    for kp in fc.keyframe_points:
                        addtime(kp.co.x)
                times.sort()
                return times
            def samplefclist(act, fclist, times, endframe):
                samples = []
                for i, t in enumerate(times):
                    kp = []
                    for fc in fclist:
                        if fc is None:
                            kp.append(0.0)
                        else:
                            kp.append(fc.evaluate(t))
                    samples.append([t - act.frame_start, kp])
                return samples
            def getsamples(act, path, ch):
                fclist = getfclist(act, path, ch)
                if fclist is None: return []
                times = gettimes(fclist)
                return samplefclist(act, fclist, times, act.frame_end)
            def getaction(name):
                nonlocal actions
                for i, a in enumerate(actions):
                    if a[0] == name: return i
                return -1
            def addbonetoact(i, act, bone):
                bpath = 'pose.bones["' + bone + '"]'
                nonlocal actions
                s = [
                    None,
                    getsamples(act, bpath + ".location", 3),
                    getsamples(act, bpath + ".rotation_euler", 3),
                    getsamples(act, bpath + ".scale", 3)
                ]
                if len(s[1]) == 0 and len(s[2]) == 0 and len(s[3]) == 0: return
                s[0] = bone
                actions[i][2].append(s)
            frametime = 1000000 * context.scene.render.fps_base / context.scene.render.fps
            for anim in armature.animation_data.nla_tracks:
                animdata = [anim.name, int(frametime), []]
                for s in anim.strips:
                    act = getaction(s.action.name)
                    if act == -1:
                        actions.append([s.action.name, s.action.frame_end - s.action.frame_start + 1, []])
                        act = len(actions) - 1
                        for b in armature.data.bones:
                            addbonetoact(act, s.action, b.name)
                    animdata[2].append([
                        act,
                        1.0 / s.scale,
                        s.action_frame_start - s.action.frame_start,
                        s.action_frame_end - s.action.frame_start
                    ])
                animations.append(animdata)

    ver = [1, 0]

    f = open(filepath, "wb")
    f.write(b"P3M\0")
    f.write(struct.pack("<B", ver[0]))
    f.write(struct.pack("<B", ver[1]))
    flags = 0b00000000
    if armature is not None:
        flags |= 0b00000001
    f.write(struct.pack("<B", flags))
    f.write(struct.pack("<H", len(vertices)))
    for v in vertices:
        f.write(struct.pack("<f", v.x))
        f.write(struct.pack("<f", v.y))
        f.write(struct.pack("<f", v.z))
    for v in vertices:
        f.write(struct.pack("<B", v.t))
        f.write(struct.pack("<f", v.u))
        f.write(struct.pack("<f", v.v))
    f.write(struct.pack("<H", len(indices)))
    for i in indices:
        f.write(struct.pack("<H", i))
    f.write(struct.pack("<B", len(materials)))
    for m in materials:
        f.write(struct.pack("<I", addtostrtable(m[1])))
    if armature is not None:
        if export_bones:
            f.write(struct.pack("<B", len(bones)))
            for b in bones:
                f.write(struct.pack("<I", addtostrtable(b[0])))
                f.write(struct.pack("<f", b[2][0]))
                f.write(struct.pack("<f", b[2][1]))
                f.write(struct.pack("<f", b[2][2]))
                f.write(struct.pack("<f", b[3][0]))
                f.write(struct.pack("<f", b[3][1]))
                f.write(struct.pack("<f", b[3][2]))
                f.write(struct.pack("<H", len(b[4])))
                for w in b[4]:
                    f.write(struct.pack("<H", w[0]))
                    f.write(struct.pack("<f", w[1]))
                f.write(struct.pack("<B", b[5]))
        else:
            f.write(struct.pack("<B", 0))
        if export_anims:
            f.write(struct.pack("<B", len(actions)))
            for a in actions:
                f.write(struct.pack("<f", a[1]))
                f.write(struct.pack("<B", len(a[2])))
                for b in a[2]:
                    f.write(struct.pack("<I", addtostrtable(b[0])))
                    f.write(struct.pack("<B", len(b[1])))
                    for d in b[1]:
                        f.write(struct.pack("<f", d[0]))
                        f.write(struct.pack("<f", d[1][0]))
                        f.write(struct.pack("<f", d[1][1]))
                        f.write(struct.pack("<f", d[1][2]))
                    f.write(struct.pack("<B", len(b[2])))
                    for d in b[2]:
                        f.write(struct.pack("<f", d[0]))
                        f.write(struct.pack("<f", d[1][0]))
                        f.write(struct.pack("<f", d[1][1]))
                        f.write(struct.pack("<f", d[1][2]))
                    f.write(struct.pack("<B", len(b[3])))
                    for d in b[3]:
                        f.write(struct.pack("<f", d[0]))
                        f.write(struct.pack("<f", d[1][0]))
                        f.write(struct.pack("<f", d[1][1]))
                        f.write(struct.pack("<f", d[1][2]))
            f.write(struct.pack("<B", len(animations)))
            for a in animations:
                f.write(struct.pack("<I", addtostrtable(a[0])))
                f.write(struct.pack("<I", a[1]))
                f.write(struct.pack("<B", len(a[2])))
                for d in a[2]:
                    f.write(struct.pack("<B", d[0]))
                    f.write(struct.pack("<f", d[1]))
                    f.write(struct.pack("<f", d[2]))
                    f.write(struct.pack("<f", d[3]))
        else:
            f.write(struct.pack("<B", 0))
            f.write(struct.pack("<B", 0))
    f.write(strtable)
    f.close()

    print("Exported " + filepath)

    return {'FINISHED'}