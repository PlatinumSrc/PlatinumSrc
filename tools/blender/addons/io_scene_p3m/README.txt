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

            User-defined fields:
                - User-defined fields can be stored to easily attach small bits of extra data to models.
                - See 'Collections' under 'Custom properties' below for more details.
                - The max length of array user-defined fields is 65536.
                - User-defined field data is usually stored in the string table which has a max length of 65536, and
                  stores all the strings used in the model. It is not recommended to use user-defined fields to store
                  large amounts of data.

        Custom properties:

            The exporter will look in these places for custom properties with these names.

            Meshes:
                - p3m:uvs - An int or bool to override the 'UVs' export option.
                - p3m:normals - An int or bool to override the 'Normals' export option.

            Materials:
                - p3m:animated - An int or bool specifying if the material is animated (defaults to 0 or false).
                - p3m:rendermode - A string of 'normal' or 'add' (defaults to 'normal').
                - p3m:color - A float (0.0 - 1.0) or int (0 - 255) array color of 3 (RGB) or 4 (RGBA) to multiply the
                  texture by (defaults to #ffffff).
                - p3m:emission - A float (0.0 - 1.0) or int (0 - 255) array color of 3 (RGB) specifying the unlit color
                  (defaults to #000000).
                - p3m:shading - A float (0.0 - 1.0) or int (0 - 255) specifying how much "shadowing" light sources do to
                  this material (defaults to max).
                - p3m:texture - A resource path string (external texture), or image or image texture object (embedded
                  texture) (defaults to no texture).
                - p3m:matcap - A resource path string (external texture), or image or image texture object (embedded
                  texture) (defaults to no matcap texture).

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

            Collections:
                - p3m:custom[:<type>]:<name> - Custom properties matching this pattern in collections containing
                  exported meshes will be added to the model's user-defined fields with the name being the text provided
                  at <name>. <type> and the preceding colon are optional, but if provided, <type> must be one of the
                  following:
                    - u<W> or i<W> where <W> is a width of 8, 16, 32, or 64 - The property data must be an integer, or a
                      string or text data-block containing a value interpretable as an integer.
                    - f32 or f64 - The property data must be a floating-point number, or a string or text data-block
                      containing a value interpretable as a floating-point number.
                    - bool - The property data must be a boolean, or a string or text data-block containing a value
                      interpretable as a boolean ("true", "false", "yes", "no", "on", "off", or an integer).
                    - str - The property data must be a string or a text data-block.
                    - u<W>[] or i<W>[] where <W> is a width of 8, 16, 32, or 64 - The property data must be a list of
                      integers, or a string or text data-block containing comma-separated values interpretable as
                      integers.
                    - f32[] or f64[] - The property data must be a list of floating-point numbers, or a string or text
                      data-block containing comma-separated values interpretable as floating-point numbers.
                    - str[] - The property data must be a list of strings, or string or text data-block containing a
                      comma-separated list where '\\' is interpreted as a literal '\' and '\,' is interpreted as a
                      literal ','.
                    - bool[] - The property data must be a list of booleans, or a string or text data-block containing
                      comma-separated values interpretable as booleans.
                  If the colon and <type> are not provided, the type will be inferred to be i32 if given an integer, f64
                  if given a floating-point number, bool if given a boolean, str if given a string or text data-block,
                  i32[] if given an integer array, f64[] if given a floating-point number array, bool[] if given a
                  boolean array, or str[] if given a string array.



