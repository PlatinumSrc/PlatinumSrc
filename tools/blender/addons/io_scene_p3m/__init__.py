bl_info = {
    "name": "PlatinumSrc P3M format",
    "author": "PQCraft",
    "version": (2, 0),
    "blender": (3, 6, 0),
    "location": "File > Import-Export",
    "description": "Import-Export PlatinumSrc P3M files",
    "doc_url": "https://platinumsrc.github.io/docs/formats/p3m/",
    "category": "Import-Export"
}

if "bpy" in locals():
    import importlib
    if "p3m" in locals():
        importlib.reload(p3m)
    if "import_p3m" in locals():
        importlib.reload(import_p3m)
    if "export_p3m" in locals():
        importlib.reload(export_p3m)

import bpy
from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    StringProperty
)
from bpy_extras.io_utils import (
    ImportHelper,
    ExportHelper
)

class ImportP3M(bpy.types.Operator, ImportHelper):
    bl_idname = "import_scene.p3m"
    bl_label = "Import P3M"
    bl_options = {'PRESET', 'UNDO'}

    filename_ext = ".p3m"
    filter_glob: StringProperty (
        default = "*.p3m",
        options = {'HIDDEN'}
    )

    def draw(self, context):
        pass

    def execute(self, context):
        from . import import_p3m
        keywords = self.as_keywords()
        return import_p3m.load(self, context, **keywords)

class ExportP3M(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.p3m"
    bl_label = "Export P3M"
    bl_options = {'PRESET'}

    filename_ext = ".p3m"
    filter_glob: StringProperty (
        default = "*.p3m",
        options = {'HIDDEN'}
    )

    export_mode: EnumProperty (
        name = "Export",
        description = "Export only visible, only selected, or all objects",
        items = [
            ('VISIBLE',  "Visible",  "Only export visible objects"),
            ('SELECTED', "Selected", "Only export selected objects"),
            ('ALL',      "All",      "Export all objects")
        ],
        default = 'VISIBLE'
    )
    export_norms: BoolProperty (
        name = "Normals",
        description = "Export normals",
        default = True
    )
    export_embtex: BoolProperty (
        name = "Embedded textures",
        description = "Export embedded textures (if false, the image's name is used instead)",
        default = True
    )
    export_bones: BoolProperty (
        name = "Bones",
        description = "Export bones",
        default = True
    )
    export_anims: BoolProperty (
        name = "Animations",
        description = "Export animations",
        default = True
    )
    scale_by: FloatProperty (
        name = "Scale by",
        description = "Scale the scene by this when exporting",
        default = 1.0,
        soft_min = 0.01,
        soft_max = 1.0,
        min = 0.0,
        step = 1
    )

    def draw(self, context):
        pass

    def execute(self, context):
        from . import export_p3m
        context.window.cursor_modal_set('WAIT')
        eout = None
        r = {'CANCELLED'}
        try:
            r = export_p3m.save(self, context, self.report, **self.as_keywords())
        except Exception as e:
            eout = e
        context.window.cursor_modal_restore()
        if eout is not None: raise eout
        return r

class P3M_PT_export_main(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Options"
    bl_parent_id = "FILE_PT_operator"
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        sfile = context.space_data
        operator = sfile.active_operator
        return operator.bl_idname == "EXPORT_SCENE_OT_p3m"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        operator = context.space_data.active_operator
        layout.prop(operator, "export_mode")
        layout.prop(operator, "export_norms")
        layout.prop(operator, "export_embtex")
        layout.prop(operator, "export_bones")
        layout.prop(operator, "export_anims")
        layout.prop(operator, "scale_by")

def menu_func_import(self, context):
    self.layout.operator(ImportP3M.bl_idname, text = "PlatinumSrc (.p3m)")

def menu_func_export(self, context):
    self.layout.operator(ExportP3M.bl_idname, text = "PlatinumSrc (.p3m)")

classes = (
    ImportP3M,
    ExportP3M,
    P3M_PT_export_main,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

    for cls in classes:
        bpy.utils.unregister_class(cls)

if __name__ == "__main__":
    register()
