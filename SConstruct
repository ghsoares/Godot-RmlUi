#!/usr/bin/env python
import os
import sys
import methods

from generate_bindings import generate_input_key_mapping

env = SConscript("godot-cpp/SConstruct")

opts = Variables([], ARGUMENTS)
opts.Add(BoolVariable("brotli", "Enable Brotli for decompression and WOFF2 fonts support", True))
opts.Add(BoolVariable("modules_enabled_by_default", "If no, disable all modules except ones explicitly enabled", True))
opts.Add(BoolVariable("svg_plugin", "Build with SVG plugin (LunaSVG)", True))

opts.Update(env)

module_name = "gdex"
bin_folder = f"#project/addons/rmlui/bin/"
lib_folder = f"#libs/{env["platform"]}.{env["target"]}.{env["arch"]}/"

methods.init_env(env, lib_folder)

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
    ["ZLIB_DEBUG"]
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
        env_neon = env.Clone()
        if "S_compiler" in env:
            env_neon["CC"] = env["S_compiler"]
        neon_sources = []
        neon_sources.append(env_neon.Object(libpng_dir + "arm/arm_init.c"))
        neon_sources.append(env_neon.Object(libpng_dir + "arm/filter_neon_intrinsics.c"))
        neon_sources.append(env_neon.Object(libpng_dir + "arm/palette_neon_intrinsics.c"))
        libpng_sources += neon_sources
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

library = env.SharedLibrary(
    "{}/lib{}{}{}".format(bin_folder, module_name, env["suffix"], env["SHLIBSUFFIX"]), 
    sources
)
Default(library)





