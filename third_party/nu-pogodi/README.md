# Original Nu, Pogodi! artwork inputs

The optional original-art build uses the original LCD segment outlines and
printed inlay mirrored by [artyomsoft/nupogodi-sdl](https://github.com/artyomsoft/nupogodi-sdl/tree/master/resources/nupogodi).

Inputs are downloaded locally by `tools/generate_nu_pogodi_art.py --fetch`:

| Input | Source | Verification |
| --- | --- | --- |
| `nupogodi.svg` | [Original LCD SVG](https://raw.githubusercontent.com/artyomsoft/nupogodi-sdl/master/resources/nupogodi/nupogodi.svg) | 154233 bytes; SHA-256 `546fdf894daa4db4cb8fdb931869f64b95fb19964bde35c05c138381b19d77ba` |
| `fg.png` | [Printed inlay](https://raw.githubusercontent.com/artyomsoft/nupogodi-sdl/master/resources/nupogodi/fg.png) | 936×593 RGBA; SHA-1 `9a2b593fc8536b25cba9b8f9152629ef6abd9744` |

The SVG also matches [MAME's Nu, Pogodi! entry](https://github.com/mamedev/mame/blob/master/src/mame/handheld/hh_sm510.cpp):
CRC32 `42cfb84a`, SHA-1 `249ca7ec78066b57f9a18e48ada64712c944e461`.
Its metadata identifies Potrace as the tracing tool. The provenance discussion
includes [hap's 2017 note about Igor's background graphics](https://forums.bannister.org/ubbthreads.php?Number=110177&ubb=showflat).

No redistribution license is stated for these two source assets. They and the
generated bitmap header are deliberately excluded from this public repository;
they do not inherit the firmware code's MIT license. Original artwork rights
remain with their respective owners. MAME's generic `artwork/LICENSE` does not
apply to this external SVG.

The importer preserves the SVG paths, transforms, and proportions. It creates
monochrome masks at the four supported screen sizes, with uniform scaling and
the original four basket poses. The game keeps its existing six-step clock;
the last of the five original LCD egg positions is held for its catch window.
The original three-digit display shows the score modulo 1000; the firmware's
top score label retains the full value. The hare is decorative in this
adaptation, and the existing three-miss rule remains in effect.

Building without imported artwork retains the mod's original pixel drawings.
Building with it requires Pillow and ImageMagick with SVG/librsvg support,
used only during asset generation; no SVG parser is added to the firmware.
