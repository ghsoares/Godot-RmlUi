#!/usr/bin/env python
import os
import sys
import misc.methods

from misc.generate_bindings import generate_input_key_mapping

ARGUMENTS['custom_api_file'] = os.path.abspath('extension_api.json')
env = SConscript("godot-cpp/SConstruct")

opts = Variables([], ARGUMENTS)
opts.Add(BoolVariable("svg_plugin", "Build with SVG plugin (LunaSVG)", True))
opts.Add(BoolVariable("element_reference_strict", "Build with strict RMLElement reference, which throws error when trying to manipulate", False))
opts.Add(BoolVariable("install_to_project_bin", "After finishing building, will install binaries in the 'project/addons/rmlui/bin' folder", False))

opts.Update(env)

env.msvc = False

module_name = "gdex"
bin_folder = f"#bin/"
lib_folder = f"#libs/{env["platform"]}.{env["target"]}.{env["arch"]}/"

misc.methods.init_env(env, lib_folder)

# Build brotli
env.build_thirdparty_library(
    "brotli", 
    [
        "common/constants.c",
        "common/context.c",
        "common/dictionary.c",
        "common/platform.c",
        "common/shared_dictionary.c",
        "common/transform.c",
        "dec/bit_reader.c",
        "dec/decode.c",
        "dec/huffman.c",
        "dec/prefix.c",
        "dec/state.c",
        "dec/static_init.c"
    ],
    ["include"],
    []
)

# Build zlib
zlib_defines = []
if env.dev_build:
    zlib_defines.append("ZLIB_DEBUG")
env.build_thirdparty_library(
    "zlib",
    [
        "adler32.c",
        "compress.c",
        "crc32.c",
        "deflate.c",
        "inffast.c",
        "inflate.c",
        "inftrees.c",
        "trees.c",
        "uncompr.c",
        "zutil.c",
    ],
    [""],
    zlib_defines
)

# Build libpng
libpng_sources = [
    "png.c",
    "pngerror.c",
    "pngget.c",
    "pngmem.c",
    "pngpread.c",
    "pngread.c",
    "pngrio.c",
    "pngrtran.c",
    "pngrutil.c",
    "pngset.c",
    "pngtrans.c",
    "pngwio.c",
    "pngwrite.c",
    "pngwtran.c",
    "pngwutil.c",
]
libpng_defines = []

if env["arch"].startswith("arm"):
    if env.msvc:  # Can't compile assembly files with MSVC.
        libpng_defines.append(("PNG_ARM_NEON_OPT", 0))
    else:
        libpng_sources.append("arm/arm_init.c")
        libpng_sources.append("arm/filter_neon_intrinsics.c")
        libpng_sources.append("arm/palette_neon_intrinsics.c")
elif env["arch"].startswith("x86"):
    libpng_defines.append("PNG_INTEL_SSE")
    libpng_sources.append("intel/intel_init.c")
    libpng_sources.append("intel/filter_sse2_intrinsics.c")
elif env["arch"] == "loongarch64":
    libpng_sources.append("iloongarch/loongarch_lsx_init.c")
    libpng_sources.append("loongarch/filter_lsx_intrinsics.c")
elif env["arch"] == "ppc64":
    libpng_sources.append("powerpc/powerpc_init.c")
    libpng_sources.append("powerpc/filter_vsx_intrinsics.c")

env.build_thirdparty_library(
    "libpng",
    libpng_sources,
    [""],
    libpng_defines
)

# Build freetype
env.build_thirdparty_library(
    "freetype",
    [
        "src/autofit/autofit.c",
        "src/base/ftbase.c",
        "src/base/ftbbox.c",
        "src/base/ftbdf.c",
        "src/base/ftbitmap.c",
        "src/base/ftcid.c",
        "src/base/ftdebug.c",
        "src/base/ftfstype.c",
        "src/base/ftgasp.c",
        "src/base/ftglyph.c",
        "src/base/ftgxval.c",
        "src/base/ftinit.c",
        "src/base/ftmm.c",
        "src/base/ftotval.c",
        "src/base/ftpatent.c",
        "src/base/ftpfr.c",
        "src/base/ftstroke.c",
        "src/base/ftsynth.c",
        "src/base/ftsystem.c",
        "src/base/fttype1.c",
        "src/base/ftwinfnt.c",
        "src/bdf/bdf.c",
        "src/bzip2/ftbzip2.c",
        "src/cache/ftcache.c",
        "src/cff/cff.c",
        "src/cid/type1cid.c",
        "src/gxvalid/gxvalid.c",
        "src/gzip/ftgzip.c",
        "src/lzw/ftlzw.c",
        "src/otvalid/otvalid.c",
        "src/pcf/pcf.c",
        "src/pfr/pfr.c",
        "src/psaux/psaux.c",
        "src/pshinter/pshinter.c",
        "src/psnames/psnames.c",
        "src/raster/raster.c",
        "src/sdf/sdf.c",
        "src/svg/svg.c",
        "src/smooth/smooth.c",
        "src/truetype/truetype.c",
        "src/type1/type1.c",
        "src/type42/type42.c",
        "src/winfonts/winfnt.c",
        "src/sfnt/sfnt.c"
    ],
    [ "include" ],
    [
        "FT2_BUILD_LIBRARY", 
        "FT_CONFIG_OPTION_USE_PNG", 
        "FT_CONFIG_OPTION_SYSTEM_ZLIB", 
        "FT_CONFIG_OPTION_USE_BROTLI"
    ]
)

if env["svg_plugin"]:
    # Build plutovg
    env.build_thirdparty_library(
        "plutovg",
        [ "source/*.c" ],
        [ "include" ],
        ["PLUTOVG_BUILD"]
    )

    # Build lunasvg
    env.build_thirdparty_library(
        "lunasvg",
        [ "source/*.cpp" ],
        [ "include" ],
        ["LUNASVG_BUILD"]
    )

# Build RmlUi
rmlui_sources = [
    "Source/Core/*.cpp",
	"Source/Core/Elements/*.cpp",
	"Source/Core/FontEngineDefault/*.cpp",
	"Source/Core/Layout/*.cpp",
]
rmlui_defines = [
    "RMLUI_STATIC_LIB",
    "RMLUI_FONT_ENGINE_FREETYPE",
    "ITLIB_FLAT_MAP_NO_THROW",
    ("RMLUI_FONT_ENGINE", "freetype"),
]
if env["svg_plugin"]:
    rmlui_sources.append("Source/SVG/*.cpp")
    rmlui_defines.append("RMLUI_SVG_PLUGIN")

env.build_thirdparty_library(
    "rmlui",
    rmlui_sources,
    ["Include"],
    rmlui_defines
)

# Build the GDExtension
source_folders = [
    "src/",
    "src/element",
    "src/input",
    "src/interface",
    "src/plugin",
    "src/rendering",
    "src/server",
]
sources = []
for folder in source_folders:
    sources += env.Glob(os.path.join(folder, '*.cpp'))
env.Append(CPPPATH=source_folders)

# Build the documentation
if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

if env["element_reference_strict"]:
    env.Append(CPPDEFINES=["ELEMENT_REFERENCE_STRICT"])

targets = []

library = env.SharedLibrary(
    "{}/lib{}{}{}".format(bin_folder, module_name, env["suffix"], env["SHLIBSUFFIX"]), 
    sources
)
targets.append(library)
if env["install_to_project_bin"]:
    targets.append(env.InstallVersionedLib(target="project/addons/rmlui/bin/", source=library))
Default(targets)





