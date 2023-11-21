bl_info = {
    "name": "PlatinumSrc P3M",
    "author": "PQCraft",
    "version": (1, 0, 0),
    "blender": (3, 6, 0),
    "location": "File > Import-Export",
    "description": "Import-Export P3M",
    "category": "Import-Export",
}

if "bpy" in locals():
    import importlib
    if "import_p3m" in locals():
        importlib.reload(import_p3m)
    if "export_p3m" in locals():
        importlib.reload(export_p3m)

import bpy
from bpy.props import (
    BoolProperty,
    StringProperty,
)
from bpy_extras.io_utils import (
    ImportHelper,
    ExportHelper,
    path_reference_mode,
    axis_conversion,
)

class ImportP3M(bpy.types.Operator, ImportHelper):
    bl_idname = "import_scene.p3m"
    bl_label = "Import P3M"
    bl_options = {'PRESET', 'UNDO'}

    filename_ext = ".p3m"
    filter_glob: StringProperty(
        default = "*.p3m",
        options = {'HIDDEN'},
    )

    def draw(self, context):
        pass

    def execute(self, context):
        from . import import_p3m
        keywords = self.as_keywords()
        return import_p3m.load(self, context, **keywords)

class ExportP3M(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.p3m"
    bl_label = 'Export P3M'
    bl_options = {'PRESET'}

    filename_ext = ".p3m"
    filter_glob: StringProperty(
        default = "*.p3m",
        options = {'HIDDEN'}
    )

    export_bones: BoolProperty(
        name = "Bones",
        description = "Export bones",
        default = True
    )
    export_anims: BoolProperty(
        name = "Animations",
        description = "Export animations",
        default = True
    )
    do_transform: BoolProperty(
        name = "Transform",
        description = "Apply object transformations",
        default = False
    )

    def draw(self, context):
        pass

    def execute(self, context):
        from . import export_p3m
        keywords = self.as_keywords()
        return export_p3m.save(self, context, **keywords)

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
        layout.prop(operator, "export_bones")
        layout.prop(operator, "export_anims")
        layout.prop(operator, "do_transform")

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
