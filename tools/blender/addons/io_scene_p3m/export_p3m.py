import bpy
import bmesh
from dataclasses import dataclass
import struct

def save(operator, context, filepath,
        export_bones, export_anims, do_transform, use_selected,
        **kwargs
    ):

    @dataclass
    class p3mvert:
        x: float
        y: float
        z: float
        u: float
        v: float
        def __eq__(self, other):
            return (
                self.x == other.x and self.y == other.y and self.z == other.z and
                self.u == other.u and self.v == other.v
            )
        def __ne__(self, other):
            return (
                self.x != other.x or self.y != other.y or self.z != other.z or
                self.u != other.u or self.v != other.v
            )

    print("Exporting " + filepath + "...")

    if bpy.ops.object.mode_set.poll(): bpy.ops.object.mode_set(mode = 'OBJECT')

    objects = context.selected_objects if use_selected else bpy.data.objects

    armature = None

    vertices = []
    indexgroups = [] # [[material index, material name], indices]
    bones = [] # [name, deform, head, tail, [index, weight], child count]
    actions = [] # [name, frames, [bone, [interp, frame, translation], [interp, frame, rotation], [interp, frame, scale]]]
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
    def addvertex(v):
        nonlocal vertices
        for i, v2 in enumerate(vertices):
            if v2 == v: return i
        vertices.append(v)
        return len(vertices) - 1
    def addindexgroup(mi, md):
        nonlocal indexgroups
        for i, g in enumerate(indexgroups):
            if g[0][0] == mi: return i
        if len(md) == 0:
            indexgroups.append([[mi, ""], []])
        else:
            indexgroups.append([[mi, md[mi].material.name], []])
        return len(indexgroups) - 1
    def addfaces(fl, md, uvl):
        nonlocal indexgroups
        for f in fl:
            g = addindexgroup(f.material_index, md)
            for l in f.loops:
                i = addvertex(p3mvert(
                    l.vert.co.x, l.vert.co.z, l.vert.co.y,
                    l[uvl].uv.x, l[uvl].uv.y
                ))
                indexgroups[g][1].append(i)
    def addmesh(obj):
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        if do_transform:
            bmesh.ops.transform(bm, matrix=obj.matrix_world, verts=bm.verts[:])
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        addfaces(bm.faces, obj.material_slots, bm.loops.layers.uv.active)
        bm.free()

    if export_bones or export_anims:
        print("Finding an armature...")
        for obj in objects:
            if not obj.hide_get() and obj.type == 'ARMATURE':
                print("Found an armature: " + obj.name)
                armature = obj
                break
        if armature is None:
            print("No armature found")

    if armature is None:
        print("Finding meshes...")
        for obj in objects:
            if not obj.hide_get() and obj.type == 'MESH':
                print("Found a mesh: " + obj.name)
                addmesh(obj)
    else:
        print("Finding meshes...")
        for obj in armature.children:
            if not obj.hide_get() and obj.type == 'MESH':
                if use_selected and not obj.select_get(): continue
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
                for i in range(count):
                    fc = act.fcurves.find(name, index=i)
                    fclist.append(fc)
                    if fc is not None: tmp += 1
                if tmp > 0: return fclist
                return None
            def gettimes(fclist):
                times = []
                def addtime(t):
                    nonlocal times
                    for i in range(len(times)):
                        if times[i][0] == t[0]:
                            if times[i][1] != t[1]: times[i][1] = 1 # default to linear if mismatch
                            return
                    times.append(t)
                for fc in fclist:
                    if fc is None: continue
                    for kp in fc.keyframe_points:
                        addtime([kp.co.x, 1 if kp.interpolation != 'CONSTANT' else 0])
                times.sort()
                return times
            def samplefclist(act, fclist, times, endframe):
                samples = []
                for t in times:
                    kp = []
                    for fc in fclist:
                        if fc is None:
                            kp.append(0.0)
                        else:
                            kp.append(fc.evaluate(t[0]))
                    samples.append([t[0] - act.frame_start, kp, t[1]])
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

    ver = [1, 1]

    f = open(filepath, "wb")
    f.write(b"P3M\0")
    f.write(struct.pack("<BB", ver[0], ver[1]))
    flags = 0b00000000
    if armature is not None:
        flags |= 0b00000001
    f.write(struct.pack("<B", flags))
    f.write(struct.pack("<H", len(vertices)))
    for v in vertices:
        f.write(struct.pack("<fff", v.x, v.y, v.z))
    for v in vertices:
        f.write(struct.pack("<ff", v.u, v.v))
    f.write(struct.pack("<B", len(indexgroups)))
    for g in indexgroups:
        f.write(struct.pack("<H", addtostrtable(g[0][1])))
        f.write(struct.pack("<H", len(g[1])))
        for i in g[1]:
            f.write(struct.pack("<H", i))
    if armature is not None:
        if export_bones:
            f.write(struct.pack("<B", len(bones)))
            for b in bones:
                f.write(struct.pack("<H", addtostrtable(b[0])))
                f.write(struct.pack("<fff", b[2][0], b[2][1], b[2][2]))
                f.write(struct.pack("<fff", b[3][0], b[3][1], b[3][2]))
                f.write(struct.pack("<H", len(b[4])))
                for w in b[4]:
                    f.write(struct.pack("<HH", w[0], int(round(w[1] * 65535))))
                f.write(struct.pack("<B", b[5]))
        else:
            f.write(struct.pack("<B", 0))
        if export_anims:
            f.write(struct.pack("<B", len(actions)))
            for a in actions:
                f.write(struct.pack("<f", a[1]))
                f.write(struct.pack("<B", len(a[2])))
                for b in a[2]:
                    f.write(struct.pack("<H", addtostrtable(b[0])))
                    f.write(struct.pack("<B", len(b[1])))
                    for d in b[1]:
                        f.write(struct.pack("<ffff", d[0], d[1][0], d[1][1], d[1][2]))
                    for d in b[1]:
                        f.write(struct.pack("<B", d[2]))
                    f.write(struct.pack("<B", len(b[2])))
                    for d in b[2]:
                        f.write(struct.pack("<ffff", d[0], d[1][0], d[1][1], d[1][2]))
                    for d in b[2]:
                        f.write(struct.pack("<B", d[2]))
                    f.write(struct.pack("<B", len(b[3])))
                    for d in b[3]:
                        f.write(struct.pack("<ffff", d[0], d[1][0], d[1][1], d[1][2]))
                    for d in b[3]:
                        f.write(struct.pack("<B", d[2]))
            f.write(struct.pack("<B", len(animations)))
            for a in animations:
                f.write(struct.pack("<H", addtostrtable(a[0])))
                f.write(struct.pack("<I", a[1]))
                f.write(struct.pack("<B", len(a[2])))
                for d in a[2]:
                    f.write(struct.pack("<Bfff", d[0], d[1], d[2], d[3]))
        else:
            f.write(struct.pack("<BB", 0, 0))
    f.write(strtable)
    f.close()

    print("Exported " + filepath)

    return {'FINISHED'}
