import bpy
import bmesh
from dataclasses import dataclass
import struct

def save(operator, context, filepath,
        global_matrix,
        export_bones, export_anims,
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

    eobj = None

    vertices = []
    indices = []
    materials = []
    strtable = "".encode("utf-8")

    def addtostrtable(s):
        nonlocal strtable
        b = (s + "\0").encode("utf-8")
        p = strtable.find(b)
        if (p == -1):
            l = len(strtable)
            strtable += b
            return l
        return p
    def addmaterial(mi, md):
        nonlocal materials
        for i, m in enumerate(materials):
            if (mi == m[0]): return i
        materials.append([mi, addtostrtable(md[mi].material.name)])
        return len(materials) - 1
    def addvertex(v):
        nonlocal vertices
        for i, v2 in enumerate(vertices):
            if (v == v2): return i
        vertices.append(v)
        return len(vertices) - 1
    def addfaces(fl, md, uvl):
        nonlocal indices
        for f in fl:
            m = addmaterial(f.material_index, md)
            for l in f.loops:
                i = addvertex(p3mvert(
                    l.vert.co.x, l.vert.co.y, l.vert.co.z,
                    m, l[uvl].uv.x, l[uvl].uv.y
                ))
                indices.append(i)
    def addmesh(obj):
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=bm.faces[:])
        addfaces(bm.faces, obj.material_slots, bm.loops.layers.uv.active)
        bm.free()

    if (export_bones or export_anims):
        print("Finding an armature...")
        for obj in bpy.data.objects:
            if (obj.type == 'ARMATURE'):
                print("Found an armature: " + obj.name)
                eobj = obj
                break

    if (eobj is None):
        for obj in bpy.data.objects:
            if (obj.type != 'MESH'): continue
            print("Found a mesh: " + obj.name)
            addmesh(obj)
    else:
        for obj in eobj.children:
            if (obj.type != 'MESH'): continue
            print("Found a mesh: " + obj.name)
            addmesh(obj)
            if (export_bones):
                def printbone(d, b):
                    print("    " + "  " * d + b.name)
                    for c in b.children:
                        printbone(d + 1, c)
                print("  Bones:")
                for bone in eobj.pose.bones:
                    if (bone.parent is None):
                        printbone(0, bone)
            if (export_anims):
                print("  Animations:")
                for anim in eobj.animation_data.nla_tracks:
                    print("    " + anim.name)
            break

    #print("vertices:", vertices)
    #print("indices:", indices)
    #print("materials:", materials)
    #print("strtable:", strtable)

    ver = [0, 0]

    f = open(filepath, "wb")
    f.write("P3M\0".encode('utf-8'))
    f.write(struct.pack("<B", ver[0]))
    f.write(struct.pack("<B", ver[1]))
    f.write(struct.pack("<B", 0))
    f.write(struct.pack("<H", len(vertices)))
    for v in vertices:
        f.write(struct.pack("<f", v.x))
        f.write(struct.pack("<f", v.y))
        f.write(struct.pack("<f", v.z))
        f.write(struct.pack("<B", v.t))
        f.write(struct.pack("<f", v.u))
        f.write(struct.pack("<f", v.v))
    f.write(struct.pack("<H", len(indices)))
    for i in indices:
        f.write(struct.pack("<H", i))
    f.write(struct.pack("<B", len(materials)))
    for m in materials:
        f.write(struct.pack("<I", m[1]))
    f.write(strtable)
    f.close()

    print("Exported " + filepath)

    return {'FINISHED'}
