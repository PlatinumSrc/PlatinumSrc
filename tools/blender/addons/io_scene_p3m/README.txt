Plugin to import and export .p3m files.

Setup:

    1. Install lz4 to Blender's Python instance. If not found, embedded textures will not be exported.
    2. Enable the 'Import-Export: PlatinumSrc P3M format' plugin.

Notes:

    Exporting:

        Notes:

            Parts:
                - One mesh object is one part.
                - Hidden parts are exported if exporting 'All' or 'Selected'. The model format stores a bitmask to
                  determine which parts are visible by default.

            Animations:
                - Only ZXY Euler rotation is supported.
                - Only linear and constant interpolation are supported.

        Custom properties:

            Meshes:
                - p3m:normals - An int or bool to override the 'Normals' export option.

            Materials:
                - p3m:rendermode - A string of 'normal' or 'add' (only has an effect for transparency) (defaults to
                  'normal').
                - p3m:texture - A resource path string (external texture), or image or image texture object (embedded
                  texture) (defaults to no texture).
                - p3m:color - A float (0.0 - 1.0) or int (0 - 255) array color of 3 (RGB) or 4 (RGBA) to multiply the
                  texture by (defaults to #ffffff).
                - p3m:emission - A float (0.0 - 1.0) or int (0 - 255) array color of 3 (RGB) specifying the unlit color
                  (defaults to #000000).
                - p3m:shading - A float (0.0 - 1.0) or int (0 - 255) specifying how much "shadowing" light sources do to
                  this material (defaults to max).

            Image textures:
                - p3m:alpha - A bool to force exporting or force not exporting the alpha channel of the image.

            Bones:
                - p3m:noexport - A bool to skip this bone when exporting bones. References to the bone in weights and
                  animations will remain.

            Actions:
                - p3m:parts - A string starting with 'none+', 'all-', '+', or '-', and containing a comma-separated list
                  of parts to determine which parts are shown during this sequence. 'none+' will only show the listed
                  parts, 'all-' will show all except the listed parts, '+' will show the listed parts in addition to the
                  defaults, and '-' will use the defaults and hide the listed parts.



