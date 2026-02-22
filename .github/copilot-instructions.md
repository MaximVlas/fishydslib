# Copilot Instructions for `fishydslib`

## Build and test commands

Use CMake from the repository root:

```bash
cmake -S . -B build
cmake --build build -j
```

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

Run a single test target (by CTest name):

```bash
ctest --test-dir build -R core_tests --output-on-failure
```

Useful build options from the main `CMakeLists.txt`:

- `-DFISHYDS_ENABLE_TESTS=ON|OFF` (default ON)
- `-DFISHYDS_ENABLE_EXAMPLES=ON|OFF` (default ON)
- `-DFISHYDS_ENABLE_BENCHMARKS=ON|OFF` (default OFF; requires Google Benchmark)
- `-DFISHYDS_USE_GLIB_ALLOC=ON|OFF` (default ON)
- `-DFISHYDS_ENABLE_SANITIZERS=ON|OFF` (default OFF)

There is no dedicated lint target in this repository; quality gates are compiler warnings in `FISHYDS_COMPILE_FLAGS` and the test suite.

## High-level architecture

`discordc` is a static C17 library assembled from layered modules:

1. `core/`: foundational primitives (`dc_status_t`, alloc hooks, `dc_string_t`, `dc_vec_t`, snowflakes, formatting, time, env, logging).
2. `json/`: yyjson wrappers and conversion helpers (`dc_json_*`, `dc_json_model_*`) that map JSON <-> typed model structs.
3. `model/`: Discord API data structures and lifecycle (`*_init`, `*_free`) for users, messages, channels, guilds, components, etc.
4. `http/`: raw HTTP + compliance helpers + REST client with Discord route bucket rate-limiting (`dc_rest_*`).
5. `gw/`: Gateway WebSocket client (`dc_gateway_*`) with heartbeat/resume/reconnect and event payload dispatch.
6. `client/`: top-level API (`dc_client_*`) composing REST + Gateway and exposing endpoint helpers.

`dc_client_create()` constructs both a REST client and a Gateway client. `dc_client_start()` fetches `/gateway/bot` through REST, then connects the gateway URL. Gateway dispatch callbacks receive raw event `d` payload JSON; typed parsing helpers for common events are in `gw/dc_events.h`.

## Key codebase conventions

- **Status-first API:** almost every operation returns `dc_status_t`; check status immediately and propagate errors.
- **Lifecycle naming:** public structs follow `*_init`/`*_free`, often with temporary-object parse patterns (`tmp`, then assign on success).
- **Ownership convention:** many `dc_client_*` output parameters explicitly overwrite existing objects without freeing prior contents first (see `@note Output overwrites ...` in `client/dc_client.h`).
- **Dual client surface:** `dc_client_*_json` functions are the raw JSON layer; typed counterparts parse into model structs/lists. Keep both layers consistent when extending endpoints.
- **Container cleanup pattern:** `dc_vec_t` stores elements by value; for vectors of complex structs, free each element in a loop before `dc_vec_free`.
- **Discord ID/permission encoding:** snowflakes and permission bitfields are strings in JSON; use helper APIs in `json/dc_json.h` rather than ad-hoc conversion.
- **Gateway callback expectations:** callbacks run on the process thread (`dc_gateway_client_process`) and should be treated as non-blocking.
- **Defensive, high-quality C:** prioritize explicit null/size/range/overflow checks, propagate `dc_status_t` errors, and preserve cleanup on all failure paths.
- **No duplicate functionality:** before adding functions, search for existing helpers/APIs with similar behavior and reuse or extend them instead of creating parallel implementations.
- **Performance + safety first:** avoid unnecessary allocations/parsing in hot paths, keep bounds checks in place, and prefer existing safe primitives (`dc_string_*`, `dc_vec_*`, checked conversions).
