from math import log2
from array import array

import bpy
import bmesh
import mathutils

from .p3m import p3m

def save(operator, context, report, filepath,
        export_mode, export_norms, export_embtex, export_bones, export_anims, scale_by,
        **kwargs
    ):
    def log(l, m, *, depth = 0):
        lt = "---"
        if l == 'DEBUG': lt = "<D>"
        elif l == 'INFO': lt = "(i)"
        elif l == 'WARNING': lt = "/!\\"
        elif l == 'ERROR' or l.startswith("ERROR_"): lt = "[E]"
        print(f"{lt}: {'  ' * depth}{m}")
        report({l}, m)

    log('DEBUG', f"Exporting {filepath}...")

    def filterobjs(objs, mode):
        match mode:
            case 'VISIBLE': return [(o, True) for o in objs if not o.hide_get()]
            case 'SELECTED': return [(o, not o.hide_get()) for o in objs if o.select_get()]
            case 'ALL': return [(o, not o.hide_get()) for o in objs]

    if bpy.ops.object.mode_set.poll(): bpy.ops.object.mode_set(mode = 'OBJECT')

    objects = filterobjs(bpy.data.objects, export_mode)
    armature = None
    meshes = []

    def emptytext(m, t):
        match m:
            case 'VISIBLE': return f"No visible {t}"
            case 'SELECTED': return f"No {t} selected"
            case 'ALL': return f"No {t} found"

    if export_bones or export_anims:
        log('DEBUG', "Finding an armature...")
        for o, v in objects:
            if o.type == 'ARMATURE':
                log('DEBUG', f"Found an armature: {o.name}")
                armature = o
                objects = filterobjs(o.children_recursive, export_mode)
        if armature is None:
            log('WARNING', emptytext(export_mode, "armature"))
            export_bones = False
            export_anims = False

    meshes = [o for o in objects if o[0].type == 'MESH']
    if len(meshes) == 0: log('WARNING', emptytext(export_mode, "meshes"))

    m = p3m()

    try:
        import lz4.frame
    except Exception as e:
        if export_embtex:
            log('ERROR', f"Embedded textures will not be exported (failed to import lz4.frame: {e})")
            export_embtex = False

    def addpart(mesh, *, visible = True):
        if len(m.parts) == p3m.PARTS_MAX:
            log('ERROR', f"Adding part '{mesh.name}' would exceed part limit of {p3m.PARTS_MAX}")
            return False
        log('DEBUG', f"Adding part '{mesh.name}'...")

        def addmat(mat):
            if mat is None:
                for i, im in enumerate(m.materials):
                    if im.name == "":
                        return i
                if len(m.materials) == p3m.MATERIALS_MAX:
                    log('ERROR', f"Adding empty material would exceed material limit of {p3m.MATERIALS_MAX}", depth = 1)
                    return -1
                log('DEBUG', f"Adding empty material...", depth = 1)
                i = len(m.materials)
                m.materials.append(p3m.material(""))
                return i

            for i, im in enumerate(m.materials):
                if im.name == mat.name:
                    log('DEBUG', f"Material '{mat.name}' already exists", depth = 1)
                    return i
            if len(m.materials) == p3m.MATERIALS_MAX:
                log('ERROR', f"Adding material '{mat.name}' would exceed material limit of {p3m.MATERIALS_MAX}", depth = 1)
                return -1
            log('DEBUG', f"Adding material '{mat.name}'...", depth = 1)

            def addembtex(tex, alpha = None):
                for i, t in enumerate(m.textures):
                    if t.type == p3m.texture.types.EMBEDDED and tex == t.data.rcpath:
                        return i
                if len(m.textures) == p3m.TEXTURES_MAX:
                    log('ERROR', f"Adding embedded texture '{tex.name}' would exceed texture limit of {p3m.TEXTURES_MAX}", depth = 2)
                    return -1
                log('DEBUG', f"Adding texture '{tex.name}'...", depth = 2)
                try:
                    assert(tex.channels == 3 or tex.channels == 4)
                    assert(log2(tex.size[0]) % 1.0 == 0.0)
                    assert(log2(tex.size[1]) % 1.0 == 0.0)
                except:
                    log('WARNING', f"Texture '{tex.name}' should be square, power of 2, and have 3 or 4 channels", depth = 3)
                    return 255
                if alpha is None:
                    if tex.channels == 4:
                        for y in range(0, tex.size[0] * tex.size[1] * 4, tex.size[0] * 4):
                            tmp = []
                            for i, v in enumerate(tex.pixels[y : y + tex.size[0] * 4]):
                                if i % 4 == 3 and v < 1.0:
                                    alpha = True
                                    break
                            if alpha: break
                        else:
                            log('INFO', f"Alpha will be automatically removed from texture '{tex.name}'", depth = 3)
                            alpha = False
                    else:
                        alpha = False
                data = array('B')
                channels = tex.channels
                if tex.channels == 3 and alpha:
                    #print("ADDING ALPHA")
                    channels = 4
                    for y in range(tex.size[0] * tex.size[1] * 3 - 1, -1, tex.size[0] * -3):
                        tmp = []
                        for i, v in enumerate(tex.pixels[y : y + tex.size[0] * 3]):
                            tmp.append(max(0, min(round(v * 255), 255)))
                            if i % 3 == 2: tmp.append(255)
                        data.extend(tmp)
                elif tex.channels == 4 and not alpha:
                    #print("REMOVING ALPHA")
                    channels = 3
                    for y in range(tex.size[0] * tex.size[1] * 4 - 1, -1, tex.size[0] * -4):
                        tmp = []
                        for i, v in enumerate(tex.pixels[y : y + tex.size[0] * 4]):
                            if i % 4 != 3: tmp.append(max(0, min(round(v * 255), 255)))
                        data.extend(tmp)
                else:
                    #print("KEEPING ALPHA")
                    for y in range(tex.size[0] * tex.size[1] * tex.channels - 1, -1, tex.size[0] * -tex.channels):
                        data.extend([max(0, min(round(v * 255), 255)) for v in tex.pixels[y : y + tex.size[0] * tex.channels]])
                l = len(m.textures)
                data = lz4.frame.compress(data, compression_level = lz4.frame.COMPRESSIONLEVEL_MAX)
                m.textures.append(p3m.texture(
                    p3m.texture.types.EMBEDDED,
                    p3m.texture.embedded(channels = channels, width = tex.size[0], height = tex.size[1], data = data)
                ))
                return l
            def addexttex(tex):
                for i, t in enumerate(m.textures):
                    if t.type == p3m.texture.types.EXTERNAL and tex == t.data.rcpath:
                        return i
                if len(m.textures) == p3m.TEXTURES_MAX:
                    log('ERROR', f"Adding external texture '{tex}' would exceed texture limit of {p3m.TEXTURES_MAX}", depth = 2)
                    return -1
                log('DEBUG', f"Adding texture '{tex}'...", depth = 2)
                tmp = addtostrtbl(tex)
                if tmp == -1: return -1
                l = len(m.textures)
                m.textures.append(p3m.texture(
                    p3m.texture.types.EMBEDDED,
                    p3m.texture.external(tex, tmp)
                ))
                return l
            def addtex(tex):
                if type(tex) == bpy.types.ImageTexture:
                    if export_embtex: return addembtex(tex.image, tex.get('p3m:alpha'))
                    return addexttex(tex.name)
                elif type(tex) == bpy.types.Image:
                    if export_embtex: return addembtex(tex)
                    return addexttex(tex.name)
                elif type(tex) == str:
                    return addexttex(tex)
                log('WARNING', f"Custom property 'p3m:texture' under material '{mat.name}' should be an image or image texture object, or a string", depth = 2)
                print(type(tex))
                return 255

            mode = mat.get('p3m:rendermode')
            if mode is None:
                mode = p3m.material.rendmodes.NORMAL
            else:
                try:
                    if type(mode) == str:
                        mode = mode.tolower()
                        if mode == "normal": mode = p3m.material.rendmodes.NORMAL
                        if mode == "add": mode = p3m.material.rendmodes.ADD
                        else: raise ValueError
                    elif type(mode) != int:
                        raise TypeError
                except:
                    log('WARNING', f"Custom property 'p3m:rendermode' under material '{mat.name}' should be an int or string of 'normal', or 'add'", depth = 2)
                    mode = p3m.material.rendmodes.NORMAL

            tex = mat.get('p3m:texture')
            if tex is None:
                tex = 255
            else:
                tex = addtex(tex)
                if tex == -1: return -1

            color = mat.get('p3m:color')
            if color is None:
                color = (255,) * 4
            else:
                try:
                    assert(len(color == 3) or len(color) == 4)
                    if type(color[0]) == float:
                        color = tuple(max(0, min(v, 255)) for v in (
                            round(color[0] * 255),
                            round(color[1] * 255),
                            round(color[2] * 255),
                            round(color[3] * 255) if len(color) == 4 else 255
                        ))
                    elif type(color[0]) == int:
                        if len(color) == 3: color = (color[0], color[1], color[2], 255)
                        color = tuple(max(0, min(v, 255)) for v in color)
                    else:
                        raise TypeError
                except:
                    log('WARNING', f"Custom property 'p3m:color' under material '{mat.name}' should be an array of 3 or 4 ints or floats", depth = 2)
                    color = (255,) * 4

            emis = mat.get('p3m:emission')
            if emis is None:
                emis = (0,) * 3
            else:
                try:
                    assert(len(emis == 3))
                    if type(emis[0]) == float:
                        emis = tuple(max(0, min(v, 255)) for v in (
                            round(emis[0] * 255),
                            round(emis[1] * 255),
                            round(emis[2] * 255)
                        ))
                    elif type(emis[0]) == int:
                        emis = tuple(max(0, min(v, 255)) for v in emis)
                    else:
                        raise TypeError
                except:
                    log('WARNING', f"Custom property 'p3m:emission' under material '{mat.name}' should be an array of 3 ints or floats", depth = 2)
                    emis = (0,) * 3

            shading = mat.get('p3m:shading')
            if shading is None:
                shading = 255
            elif type(shading) == float:
                shading = max(0, min(round(shading * 255), 255))
            elif type(shading) != int:
                log('WARNING', f"Custom property 'p3m:shading' under material '{mat.name}' should be an int", depth = 2)
                shading = 255

            i = len(m.materials)
            m.materials.append(p3m.material(mat.name, rendmode = mode, tex = tex, color = color, emis = emis, shading = shading))
            return i

        tmp = addtostrtbl(mesh.name)
        if tmp == -1: return False
        outp = p3m.part(mesh.name, tmp)
        tmp = addmat(mesh.active_material)
        if tmp == -1: return False
        outp.mat = tmp
        del tmp
        bm = bmesh.new()
        bm.from_mesh(mesh.data)
        #bm.verts.ensure_lookup_table()
        if armature is None:
            bmesh.ops.transform(bm, matrix = mesh.matrix_world, verts = bm.verts)
        else:
            mat = mesh.matrix_local.copy()
            o = mesh.parent
            while o != armature:
                mat @= o.matrix_local
                o = o.parent
            mat @= mathutils.Matrix.LocRotScale(armature.location, None, armature.scale)
            bmesh.ops.transform(bm, matrix = mat, verts = bm.verts)
        bmesh.ops.triangulate(bm, faces = bm.faces)
        uvl = bm.loops.layers.uv.active
        indmap = []
        if mesh.get('p3m:normals', export_norms):
            outp.flags |= p3m.part.flags.HASNORMALS
            for f in bm.faces:
                for l in f.loops:
                    vert = l.vert
                    x = vert.co.x; y = vert.co.z; z = vert.co.y
                    u = l[uvl].uv.x; v = l[uvl].uv.y
                    nx = vert.normal.x; ny = vert.normal.z; nz = vert.normal.y
                    end = len(outp.verts) // 5
                    for i in range(end):
                        vi = i * 5
                        ni = i * 3
                        if (
                            outp.verts[vi] == x and outp.verts[vi + 1] == y and outp.verts[vi + 2] == z and
                            outp.verts[vi + 3] == u and outp.verts[vi + 4] == v and
                            outp.norms[ni] == nx and outp.norms[ni + 1] == ny and outp.norms[ni + 2] == nz
                        ):
                            outp.inds.append(i)
                            break
                    else:
                        outp.verts.extend((x, y, z, u, v))
                        outp.norms.extend((nx, ny, nz))
                        indmap.append(vert.index)
                        outp.inds.append(end)
        else:
            for f in bm.faces:
                for l in f.loops:
                    vert = l.vert
                    x = vert.co.x; y = vert.co.z; z = vert.co.y
                    u = l[uvl].uv.x; v = l[uvl].uv.y
                    end = len(outp.verts)
                    for i in range(0, end, 5):
                        if (
                            outp.verts[vi] == x and outp.verts[vi + 1] == y and outp.verts[vi + 2] == z and
                            outp.verts[vi + 3] == u and outp.verts[vi + 4] == v
                        ):
                            outp.inds.append(i // 5)
                            break
                    else:
                        outp.verts.extend((x, y, z, u, v))
                        indmap.append(vert.index)
                        outp.inds.append(end // 5)
        #log('DEBUG', f"len(verts) = {len(mesh.data.vertices)} -> {len(bm.verts)} -> {len(outp.verts) // 5}", depth = 1)
        if export_bones:
            for g in mesh.vertex_groups:
                i = 0
                l = len(indmap)
                while i < l:
                    try:
                        v = round(g.weight(indmap[i]) * 256)
                        if v != 0: break
                    except:
                        pass
                    i += 1
                else:
                    continue
                w = [0] * i
                while i < l:
                    try:
                        v = round(g.weight(indmap[i]) * 256)
                        w.append(v)
                    except:
                        w.append(0)
                    i += 1
                i = 0
                b = 0
                o = []
                while True:
                    while i < l and w[i] == 0: i += 1
                    s = i - b
                    b = i
                    while i < l and w[i] != 0: i += 1
                    o.append((s, array('B', [v - 1 for v in w[b : i]])))
                    if b == i: break
                    b = i
                tmp = addtostrtbl(g.name)
                if tmp == -1: return False
                outp.weights.append((tmp, o))
        else:
            del indmap
        bm.free()
        del bm
        m.vismask[len(m.parts) // 8] |= visible << (len(m.parts) % 8)
        m.parts.append(outp)
        return True

    def addbone(b): return _addbone(0, b)
    def _addbone(l, b):
        if len(m.bones) == p3m.BONES_MAX:
            log('ERROR', f"Adding bone '{b.name}' would exceed bone limit of {p3m.BONES_MAX}", depth = l)
            return False
        #def strvec(v):
        #    return f"({round(v[0], 4) :g}, {round(v[1], 4) :g}, {round(v[2], 4) :g})"
        mat = mathutils.Matrix.LocRotScale(armature.location, None, armature.scale)
        #log('DEBUG', f"Adding bone '{b.name}' (head = {strvec(mat @ b.head_local)}, tail = {strvec(mat @ b.tail_local)})...", depth = l)
        log('DEBUG', f"Adding bone '{b.name}'...", depth = l)
        tmp = addtostrtbl(b.name)
        if tmp == -1: return False
        outb = p3m.bone(b.name, tmp, head = mat @ b.head_local, tail = mat @ b.tail_local, childct = len(b.children))
        del tmp
        m.bones.append(outb)
        for c in b.children:
            if not _addbone(l + 1, c): return False
        return True

    def addanim(track, *, frameus: int):
        if len(m.animations) == p3m.ANIMATIONS_MAX:
            log('ERROR', f"Adding animation '{track.name}' would exceed limit of {p3m.ANIMATIONS_MAX}")
            return False
        log('DEBUG', f"Adding animation '{track.name}'...")

        def addact(act) -> int:
            for i, a in enumerate(m.actions):
                if a.name == act.name:
                    log('DEBUG', f"Action exists '{act.name}'", depth = 1)
                    return i
            if len(m.actions) == p3m.ACTIONS_MAX:
                log('ERROR', f"Adding action '{act.name}' would exceed action limit of {p3m.ACTIONS_MAX}", depth = 1)
                return -1
            log('DEBUG', f"Adding action '{act.name}'...", depth = 1)
            outa = p3m.action(act.name, frameus = frameus)
            if (parts := act.get('p3m:parts')) is not None:
                if type(parts) == str:
                    parts = parts.lstrip()
                    if parts.startswith("none+"):
                        outa.partvis = p3m.action.partvis.WHITE
                        parts = parts[5:]
                    elif parts.startswith("all-"):
                        outa.partvis = p3m.action.partvis.BLACK
                        parts = parts[4:]
                    elif parts.startswith("+"):
                        outa.partvis = p3m.action.partvis.DEFAULTWHITE
                        parts = parts[1:]
                    elif parts.startswith("-"):
                        outa.partvis = p3m.action.partvis.DEFAULTBLACK
                        parts = parts[1:]
                    else:
                        log('WARNING', f"Custom property 'p3m:parts' under action '{act.name}' should start with 'none+', 'all-', '+', or '-'", depth = 2)
                        parts = ""
                    if parts:
                        cur = ""
                        i = 0
                        e = len(parts)
                        while i < e:
                            if parts[i] == ',':
                                outa.partnames.append(cur)
                                tmp = addtostrtbl(cur)
                                if tmp == -1: return -1
                                outa.partnameinds.append(tmp)
                                del tmp
                                cur = ""
                            elif parts[i] == '\\' and i < e - 1:
                                i += 1
                                if parts[i] == '\\': cur += parts[i]
                                elif parts[i] == ',': cur += ','
                                else: cur += '\\' + parts[i]
                            else:
                                cur += parts[i]
                            i += 1
                        outa.partnames.append(cur)
                        tmp = addtostrtbl(cur)
                        if tmp == -1: return -1
                        outa.partnameinds.append(tmp)
                else:
                    log('WARNING', f"Custom property 'p3m:parts' under action '{act.name}' should be a string", depth = 2)
            # TODO: add action data
            curvelists = {}
            def pushcurve(b, i, c):
                #print(b, i, c.array_index)
                cl = curvelists.get(b)
                if cl is None: cl = [[None, None, None], [None, None, None], [None, None, None]]
                cl[i][c.array_index] = c
                curvelists[b] = cl
            for c in act.fcurves:
                if (c.data_path.startswith('pose.bones["')):
                    if (i := c.data_path.rfind('"].location', 12)) != -1: pushcurve(c.data_path[12 : i], 0, c)
                    elif (i := c.data_path.rfind('"].rotation_euler', 12)) != -1: pushcurve(c.data_path[12 : i], 1, c)
                    elif (i := c.data_path.rfind('"].scale', 12)) != -1: pushcurve(c.data_path[12 : i], 2, c)
            framestart = int(act.frame_range[0])
            for b, cl in curvelists.items():
                def evalframes(g, d, skips, interps, data):
                    fl = set()
                    fi = {}
                    for j in range(3):
                        if g[j] is None: continue
                        for k in g[j].keyframe_points:
                            v = round(k.co[0]) - framestart
                            fl.add(v)
                            fi[v] = int(k.interpolation != 'CONSTANT')
                    fl = list(fl)
                    fl.sort()
                    last = -1
                    lastinterp = 1
                    for cur in fl:
                        if cur >= 0:
                            while (diff := cur - last) > 256:
                                last += 256
                                skips.append(255)
                                interps.append(lastinterp)
                                data.append(g[0].evaluate(last) if g[0] is not None else d)
                                data.append(g[1].evaluate(last) if g[1] is not None else d)
                                data.append(g[2].evaluate(last) if g[2] is not None else d)
                            skips.append(diff - 1)
                            interps.append(lastinterp)
                            data.append(g[0].evaluate(cur) if g[0] is not None else d)
                            data.append(g[1].evaluate(cur) if g[1] is not None else d)
                            data.append(g[2].evaluate(cur) if g[2] is not None else d)
                            last = cur
                        lastinterp = fi[cur]
                tmp = addtostrtbl(b)
                if tmp == -1: return -1
                outdata = p3m.action.data(b, tmp)
                evalframes(cl[0], 0.0, outdata.transframeskips, outdata.transinterps, outdata.transdata)
                if len(outdata.transframeskips) > p3m.action.data.KEYFRAMES_MAX:
                    log('ERROR', f"Translation keyframes for bone '{b}' under action '{act.name}' exceeds keyframe limit of {p3m.action.data.KEYFRAMES_MAX}", depth = 2)
                    return -1
                evalframes(cl[1], 0.0, outdata.rotframeskips, outdata.rotinterps, outdata.rotdata)
                if len(outdata.rotframeskips) > p3m.action.data.KEYFRAMES_MAX:
                    log('ERROR', f"Rotation keyframes for bone '{b}' under action '{act.name}' exceeds keyframe limit of {p3m.action.data.KEYFRAMES_MAX}", depth = 2)
                    return -1
                evalframes(cl[2], 1.0, outdata.scaleframeskips, outdata.scaleinterps, outdata.scaledata)
                if len(outdata.scaleframeskips) > p3m.action.data.KEYFRAMES_MAX:
                    log('ERROR', f"Scale keyframes for bone '{b}' under action '{act.name}' exceeds keyframe limit of {p3m.action.data.KEYFRAMES_MAX}", depth = 2)
                    return -1
                outa.data.append(outdata)
                del tmp
            i = len(m.actions)
            m.actions.append(outa)
            return i

        tmp = addtostrtbl(track.name)
        if tmp == -1: return False
        outa = p3m.animation(track.name, tmp)
        for s in track.strips:
            tmp = addact(s.action)
            if tmp == -1: return False
            a = p3m.animation.action(
                tmp,
                speed = 1.0 / s.scale,
                start = int(s.action_frame_start) - int(s.action.frame_range[0]),
                end = int(s.action_frame_end) - int(s.action.frame_range[0])
            )
            outa.data.append(a)
        m.animations.append(outa)
        return True

    def addtostrtbl(s) -> int:
        b = s.encode("utf-8") + b"\0"
        p = m.strtbl.find(b)
        if p == -1:
            l = len(m.strtbl)
            if l + len(b) > p3m.STRINGTABLE_MAX:
                log('ERROR', f"String table reached limit of {p3m.STRINGTABLE_MAX}")
                return -1
            m.strtbl += b
            return l
        return p

    for o in meshes:
        if not addpart(o[0], visible = o[1]): return {'CANCELLED'}
    if export_bones:
        for b in [b for b in armature.data.bones if b.parent is None and not b.get('p3m:noexport', False)]:
            if not addbone(b): return {'CANCELLED'}
    if export_anims:
        frameus = int(1000000 * context.scene.render.fps_base) // context.scene.render.fps
        for t in armature.animation_data.nla_tracks:
            if not addanim(t, frameus = frameus): return {'CANCELLED'}

    try:
        f = open(filepath, "wb")
    except Exception as e:
        log('ERROR', f"Could not open output file: {e}")
        return {'CANCELLED'}
    eout = None
    try:
        m.tofile(f)
    except Exception as e:
        log('ERROR', f"Failed to export {filepath}")
        eout = e
    f.close()
    if eout is not None: raise eout

    log('DEBUG', f"Exported {filepath}")

    return {'FINISHED'}
