# fishydslib

WIP C17 Discord library (REST + Gateway). This project is incomplete and the API is unstable.

Built/tested on CachyOS (Arch Linux).

## Dependencies (tested versions)

- CMake: 4.2.3
- GCC: 15.2.1
- libcurl: 8.18.0
- libwebsockets: 4.4.0
- yyjson: 0.12.0
- zlib: 1.3.1 (zlib-ng on my system)
- glib-2.0: 2.86.3 (optional; enabled by default)

## Build

```bash
cmake -S . -B build
cmake --build build
```

Options (see `CMakeLists.txt`):

- `-DFISHYDS_ENABLE_TESTS=ON|OFF` (default: ON)
- `-DFISHYDS_ENABLE_EXAMPLES=ON|OFF` (default: ON)
- `-DFISHYDS_ENABLE_BENCHMARKS=ON|OFF` (default: OFF, requires Google Benchmark)
- `-DFISHYDS_USE_GLIB_ALLOC=ON|OFF` (default: ON)
- `-DFISHYDS_ENABLE_SANITIZERS=ON|OFF` (default: OFF)

## Output / Linking

The library target is a static library named `discordc`.

If you install it via CMake, headers install under `include/fishydslib/`.

## Docs / Status

- API reference: `FishyDsLib-API-Reference.mdx`
- Project notes/tracking live in the repo root (`tasks.md`, `plan.md`).

## License

See `LICENSE`.
