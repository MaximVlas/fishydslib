# fishydslib

`fishydslib` is a C17 Discord library targeting Discord API v10. The repository currently contains REST, Gateway, JSON, model, and client layers, with an API surface that is still evolving.

The codebase is Linux-first. Windows support is present and validated, but the build and runtime conventions continue to follow the same behavior-safety and portability requirements used by the Linux/POSIX implementation.

## Table of Contents

- [Status](#status)
- [Build System Overview](#build-system-overview)
- [Dependency Resolution](#dependency-resolution)
- [Build Configuration](#build-configuration)
- [Platform Notes](#platform-notes)
- [Testing](#testing)
- [Installation and CMake Export](#installation-and-cmake-export)
- [Documentation](#documentation)
- [License](#license)

## Status

- Language standard: C17
- Primary library target: `discordc` (static library)
- Public headers install under: `include/fishydslib/`
- CMake package export namespace: `fishydslib::`
- Repository maturity: work in progress, API not yet stable

## Build System Overview

The top-level build is driven by CMake and exposes a compiler-selection cache variable:

| Cache Variable | Type | Default | Description |
|---|---|---:|---|
| `FISHYDS_C_COMPILER` | `STRING` | `auto` | Preferred compiler alias or executable path. |

Recognized compiler aliases:

| Alias | Meaning |
|---|---|
| `auto` | Leave compiler selection to the environment/CMake defaults. |
| `clang` | Use `clang` if discovered. |
| `gcc` | Use `gcc` if discovered. |
| `cc` | Use `cc` if discovered. |
| `zig-cc` | Use `zig cc`. |
| `clang-cl` | Use `clang-cl` if discovered. |
| `cl` | Use MSVC `cl` if discovered. |

During configure, the build prints the discovered compiler aliases and the final selected compiler frontend. Warning, hardening, and sanitizer flags are probed before use, so unsupported flags are skipped instead of being forced onto unrelated toolchains.

A minimal configure/build sequence is:

```bash
cmake -S . -B build
cmake --build build
```

Compiler selection examples:

```bash
cmake -S . -B build-clang -DFISHYDS_C_COMPILER=clang
cmake -S . -B build-gcc -DFISHYDS_C_COMPILER=gcc
cmake -S . -B build-zig -DFISHYDS_C_COMPILER=zig-cc
cmake -S . -B build-custom -DFISHYDS_C_COMPILER=/full/path/to/compiler
```

Native CMake compiler variables remain valid. For Zig, the equivalent explicit form is:

```bash
cmake -S . -B build-zig \
  -DCMAKE_C_COMPILER=zig \
  -DCMAKE_C_COMPILER_ARG1=cc
```

## Dependency Resolution

Dependency discovery is centralized in `cmake/fishyds_dependencies.cmake`.

### Required Dependencies

| Dependency | Resolution Strategy |
|---|---|
| `libcurl` | `find_package(CURL)` or explicit `CURL_LIBRARY` / `CURL_INCLUDE_DIR` |
| `libwebsockets` | `pkg-config`, `find_package(libwebsockets CONFIG)`, or explicit include/library path |
| `yyjson` | `pkg-config`, `find_package(yyjson CONFIG)`, explicit include/library path, or auto-fetch when enabled |
| `zlib` | `find_package(ZLIB)` or auto-fetch when enabled |

### Optional Dependency

| Dependency | Guard | Notes |
|---|---|---|
| `glib-2.0` | `FISHYDS_USE_GLIB_ALLOC=ON` | Only required when GLib-backed allocators are enabled. |

### Auto-Fetch Behavior

`FISHYDS_FETCH_MISSING_DEPS` controls release-archive fallback for selected dependencies:

| Option | Default | Effect |
|---|---:|---|
| `FISHYDS_FETCH_MISSING_DEPS` | `ON` | Allows CMake to fetch `yyjson` and `zlib` release archives when local packages are not available. |

Auto-fetch currently applies to:

- `yyjson`
- `zlib`

Auto-fetch does **not** currently apply to:

- `libcurl`
- `libwebsockets`

Those dependencies still require a local package-manager install, a CMake package/config, `pkg-config`, or explicit include/library paths.

Manual override variables currently recognized by the build:

| Variable | Description |
|---|---|
| `FISHYDS_LIBWEBSOCKETS_INCLUDE_DIR` | `libwebsockets` include directory override |
| `FISHYDS_LIBWEBSOCKETS_LIBRARY` | `libwebsockets` library override |
| `FISHYDS_YYJSON_INCLUDE_DIR` | `yyjson` include directory override |
| `FISHYDS_YYJSON_LIBRARY` | `yyjson` library override |
| `FISHYDS_GLIB_INCLUDE_DIR` | GLib include directory override |
| `FISHYDS_GLIB_CONFIG_INCLUDE_DIR` | GLib config include directory override |
| `FISHYDS_GLIB_LIBRARY` | GLib library override |

## Build Configuration

### Common Build Options

| Option | Default | Description |
|---|---:|---|
| `FISHYDS_ENABLE_TESTS` | `ON` | Build the test suite and register CTest entries. |
| `FISHYDS_ENABLE_EXAMPLES` | `OFF` | Build the `examples/` subtree when present. |
| `FISHYDS_ENABLE_BENCHMARKS` | `OFF` | Build benchmarks; enables C++ and expects the benchmark subtree. |
| `FISHYDS_ENABLE_SANITIZERS` | `OFF` | Probe and enable supported sanitizer compile/link flags. |
| `FISHYDS_USE_GLIB_ALLOC` | `OFF` | Build `dc_alloc` on top of GLib allocation hooks. |
| `FISHYDS_FETCH_MISSING_DEPS` | `ON` | Fetch supported missing dependencies (`yyjson`, `zlib`). |

### Typical Linux / POSIX Configure

```bash
cmake -S . -B build-linux -DFISHYDS_C_COMPILER=clang
cmake --build build-linux
ctest --test-dir build-linux --output-on-failure
```

On Unix platforms, the build adds `_POSIX_C_SOURCE=200809L` to the `discordc` target in order to enable the expected POSIX interfaces used by the library internals.

## Platform Notes

### Linux / POSIX

The primary development and compatibility target remains Linux/POSIX. Compiler and runtime changes introduced for Windows support are intended to avoid altering existing Linux/POSIX behavior.

### Windows

Windows support is validated with a single-config generator and `zig cc`.

Validated configure/build/test sequence:

```powershell
Remove-Item -Recurse -Force build-zig-win -ErrorAction SilentlyContinue
cmake -S . -B build-zig-win -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DFISHYDS_C_COMPILER=zig-cc
cmake --build build-zig-win
ctest --test-dir build-zig-win --output-on-failure
```

Windows-specific build rules currently enforced by the repository:

| Rule | Rationale |
|---|---|
| `zig-cc` with the Visual Studio generator is rejected | MSBuild takes ownership of the link/archive flow and does not cooperate with the intended Zig toolchain path. |
| `zig-cc` configures `CMAKE_AR` / `CMAKE_RANLIB` through Zig subcommands | Static archive creation must remain within the Zig/LLVM toolchain when Zig is the selected C compiler. |
| CTest receives `vcpkg_installed/<triplet>/bin` and `debug/bin` on `PATH` | Test executables on Windows may depend on runtime DLLs supplied by vcpkg packages. |

Additional Windows notes:

- PowerShell path expansion uses `$env:VCPKG_ROOT`, not `%VCPKG_ROOT%`.
- Reusing a build directory created before `vcpkg.json` existed or before manifest mode was active may leave vcpkg in classic mode. A clean build directory or a removed `CMakeCache.txt` resolves that condition.
- `vcpkg.json` is intended to provide `curl`, `libwebsockets`, `yyjson`, and `zlib` automatically when the vcpkg toolchain is active.
- vcpkg-based Windows builds still require the Visual C++ build tools and a Windows SDK because Windows ports depend on SDK tools such as `rc.exe` and `mt.exe`.

## Testing

When `FISHYDS_ENABLE_TESTS=ON`, the repository builds and registers the following CTest targets:

| CTest Name | Executable |
|---|---|
| `core_tests` | `test_core` |
| `json_tests` | `test_json` |
| `http_tests` | `test_http` |
| `rest_tests` | `test_rest` |
| `gateway_tests` | `test_gateway` |
| `events_expansion_tests` | `test_events_expansion` |
| `client_api_tests` | `test_client_api` |

Standard test execution:

```bash
ctest --test-dir build --output-on-failure
```

Focused execution examples:

```bash
ctest --test-dir build --output-on-failure -R json_tests
ctest --test-dir build --output-on-failure -R client_api_tests
```

The `client_api_tests` target is part of the default registered suite and exercises the `dc_client` API surface, including null-guard coverage.

## Installation and CMake Export

Installation exports the static library, headers, and CMake package metadata.

Install example:

```bash
cmake --install build --prefix /desired/prefix
```

Installed layout:

| Path | Contents |
|---|---|
| `lib/` | `discordc` static archive |
| `bin/` | Runtime outputs when applicable |
| `bin/tests/` | Installed test executables |
| `include/fishydslib/` | Public headers from `core/`, `json/`, `http/`, `gw/`, `model/`, and `client/` |
| `lib/cmake/fishydslib/` | `fishydslibTargets.cmake` and version metadata |

Exported target namespace:

```cmake
find_package(fishydslib CONFIG REQUIRED)
target_link_libraries(example PRIVATE fishydslib::discordc)
```

## Documentation

Repository documentation currently includes:

| File | Description |
|---|---|
| `FishyDsLib-API-Reference.mdx` | Public API reference for exported headers |
| `README.md` | Build, dependency, test, and installation overview |

## License

See `LICENSE`.
