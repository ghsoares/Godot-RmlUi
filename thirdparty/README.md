## brotli

- Upstream: https://github.com/google/brotli
- Version: 1.2.0 (028fb5a23661f123017c060daa546b55cf4bde29, 2025)
- License: MIT

Files extracted from upstream source:

- `common/`, `dec/` and `include/` folders from `c/`,
  minus the `dictionary.bin*` files
- `LICENSE`


## freetype

- Upstream: https://gitlab.freedesktop.org/freetype/freetype
- Version: 2.14.1 (526ec5c47b9ebccc4754c85ac0c0cdf7c85a5e9b, 2025)
- License: FreeType License (BSD-like)

Files extracted from upstream source:

- `src/` folder, minus the `dlg` and `tools` subfolders
  * These files can be removed: `.dat`, `.diff`, `.mk`, `.rc`, `README*`
  * In `src/gzip/`, keep only `ftgzip.c`
- `include/` folder, minus the `dlg` subfolder
- `LICENSE.TXT` and `docs/FTL.TXT`


## libpng

- Upstream: http://libpng.org/pub/png/libpng.html
- Version: 1.6.53 (4e3f57d50f552841550a36eabbb3fbcecacb7750, 2025)
- License: libpng/zlib

Files extracted from upstream source:

- All `.c` and `.h` files of the main directory, apart from `example.c` and `pngtest.c`
- `arm/`, `intel/`, `loongarch/`, and `powerpc/` folders, except `arm/filter_neon.S` and `.editorconfig` files
- `scripts/pnglibconf.h.prebuilt` as `pnglibconf.h`
- `LICENSE`


## lunasvg

- Upstream: https://github.com/sammycage/lunasvg
- Version: v3.5.0 (83c58df8103dc7dca423dfd824992af94d49bed6, 2025)
- License: MIT

Files extracted from upstream source:

- `include` and `source` folders
- `LICENSE`


## plutovg

- Upstream: https://github.com/sammycage/plutovg
- Version: v1.3.2 (5695a711dd1cff1f01fa6542f3fe6a15de082c63, 2025)
- License: MIT

Files extracted from upstream source:

- `include` and `source` folders
- `LICENSE`


## rmlui

- Upstream: https://github.com/mikke89/RmlUi
- Version: 6.3 (a5ceb26e577b554d074b8a5efe0d4f459adf7970, 2026)
- License: MIT

Files extracted from upstream source:

- `Include` and `Source` folders, except `Include/RmlUi/Debugger` and `Source/Debugger` folders
- `LICENSE.txt`
- Made change to Source/Core/LogDefault.cpp, removing debug log for default log interface.


## zlib

- Upstream: https://github.com/madler/zlib
- Version: 1.3.1.2 (570720b0c24f9686c33f35a1b3165c1f568b96be, 2025)
- License: zlib

Files extracted from upstream source:

- All `.c` and `.h` files, except `gz*.c` and `infback.c`
- `LICENSE`
