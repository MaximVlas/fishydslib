cmake_minimum_required(VERSION 3.16)

set(FISHYDS_C_COMPILER "auto" CACHE STRING
    "Preferred C compiler alias or executable path (auto, clang, gcc, cc, zig-cc, clang-cl, cl)")
set_property(CACHE FISHYDS_C_COMPILER PROPERTY STRINGS auto clang gcc cc zig-cc clang-cl cl)

find_program(FISHYDS_CLANG_C_COMPILER NAMES clang)
find_program(FISHYDS_GCC_C_COMPILER NAMES gcc)
find_program(FISHYDS_CC_C_COMPILER NAMES cc)
find_program(FISHYDS_ZIG_EXECUTABLE NAMES zig)
find_program(FISHYDS_CLANG_CL_C_COMPILER NAMES clang-cl)
find_program(FISHYDS_CL_C_COMPILER NAMES cl)

set(FISHYDS_DISCOVERED_C_COMPILERS "")
if(FISHYDS_CLANG_C_COMPILER)
    list(APPEND FISHYDS_DISCOVERED_C_COMPILERS clang)
endif()
if(FISHYDS_GCC_C_COMPILER)
    list(APPEND FISHYDS_DISCOVERED_C_COMPILERS gcc)
endif()
if(FISHYDS_CC_C_COMPILER)
    list(APPEND FISHYDS_DISCOVERED_C_COMPILERS cc)
endif()
if(FISHYDS_ZIG_EXECUTABLE)
    list(APPEND FISHYDS_DISCOVERED_C_COMPILERS zig-cc)
endif()
if(FISHYDS_CLANG_CL_C_COMPILER)
    list(APPEND FISHYDS_DISCOVERED_C_COMPILERS clang-cl)
endif()
if(FISHYDS_CL_C_COMPILER)
    list(APPEND FISHYDS_DISCOVERED_C_COMPILERS cl)
endif()

if(NOT DEFINED CMAKE_C_COMPILER AND "$ENV{CC}" STREQUAL "" AND NOT FISHYDS_C_COMPILER STREQUAL "auto")
    if(EXISTS "${FISHYDS_C_COMPILER}")
        set(CMAKE_C_COMPILER "${FISHYDS_C_COMPILER}" CACHE FILEPATH "C compiler selected by fishydslib" FORCE)
        get_filename_component(_fishyds_compiler_name "${FISHYDS_C_COMPILER}" NAME)
        if(_fishyds_compiler_name STREQUAL "zig" OR _fishyds_compiler_name STREQUAL "zig.exe")
            set(CMAKE_C_COMPILER_ARG1 "cc" CACHE STRING "Compiler argument selected by fishydslib" FORCE)
        else()
            unset(CMAKE_C_COMPILER_ARG1 CACHE)
        endif()
    elseif(FISHYDS_C_COMPILER STREQUAL "clang" AND FISHYDS_CLANG_C_COMPILER)
        set(CMAKE_C_COMPILER "${FISHYDS_CLANG_C_COMPILER}" CACHE FILEPATH "C compiler selected by fishydslib" FORCE)
    elseif(FISHYDS_C_COMPILER STREQUAL "gcc" AND FISHYDS_GCC_C_COMPILER)
        set(CMAKE_C_COMPILER "${FISHYDS_GCC_C_COMPILER}" CACHE FILEPATH "C compiler selected by fishydslib" FORCE)
    elseif(FISHYDS_C_COMPILER STREQUAL "cc" AND FISHYDS_CC_C_COMPILER)
        set(CMAKE_C_COMPILER "${FISHYDS_CC_C_COMPILER}" CACHE FILEPATH "C compiler selected by fishydslib" FORCE)
    elseif(FISHYDS_C_COMPILER STREQUAL "zig-cc" AND FISHYDS_ZIG_EXECUTABLE)
        set(CMAKE_C_COMPILER "${FISHYDS_ZIG_EXECUTABLE}" CACHE FILEPATH "C compiler selected by fishydslib" FORCE)
        set(CMAKE_C_COMPILER_ARG1 "cc" CACHE STRING "Compiler argument selected by fishydslib" FORCE)
    elseif(FISHYDS_C_COMPILER STREQUAL "clang-cl" AND FISHYDS_CLANG_CL_C_COMPILER)
        set(CMAKE_C_COMPILER "${FISHYDS_CLANG_CL_C_COMPILER}" CACHE FILEPATH "C compiler selected by fishydslib" FORCE)
        unset(CMAKE_C_COMPILER_ARG1 CACHE)
    elseif(FISHYDS_C_COMPILER STREQUAL "cl" AND FISHYDS_CL_C_COMPILER)
        set(CMAKE_C_COMPILER "${FISHYDS_CL_C_COMPILER}" CACHE FILEPATH "C compiler selected by fishydslib" FORCE)
        unset(CMAKE_C_COMPILER_ARG1 CACHE)
    else()
        message(FATAL_ERROR
            "Requested FISHYDS_C_COMPILER='${FISHYDS_C_COMPILER}', but it was not found. "
            "Discovered compiler aliases: ${FISHYDS_DISCOVERED_C_COMPILERS}")
    endif()
endif()

if(WIN32
   AND FISHYDS_C_COMPILER STREQUAL "zig-cc"
   AND CMAKE_GENERATOR MATCHES "Visual Studio")
    message(FATAL_ERROR
        "FISHYDS_C_COMPILER=zig-cc is not supported with the Visual Studio generator on Windows. "
        "Reconfigure with -G Ninja (recommended) or another single-config generator so zig cc "
        "drives both compile and link steps.")
endif()

project(fishydslib VERSION 0.1.0 LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_filename_component(_fishyds_selected_c_compiler_name "${CMAKE_C_COMPILER}" NAME)
set(FISHYDS_USING_ZIG_CC OFF)
if(_fishyds_selected_c_compiler_name STREQUAL "zig" OR _fishyds_selected_c_compiler_name STREQUAL "zig.exe")
    set(FISHYDS_USING_ZIG_CC ON)
endif()

if(FISHYDS_USING_ZIG_CC)
    set(CMAKE_AR "${CMAKE_C_COMPILER}" CACHE FILEPATH
        "Archiver used by fishydslib when zig cc is selected" FORCE)
    set(CMAKE_RANLIB "${CMAKE_C_COMPILER}" CACHE FILEPATH
        "Ranlib used by fishydslib when zig cc is selected" FORCE)
    set(CMAKE_C_COMPILER_AR "${CMAKE_C_COMPILER}" CACHE FILEPATH
        "Compiler archiver used by fishydslib when zig cc is selected" FORCE)
    set(CMAKE_C_COMPILER_RANLIB "${CMAKE_C_COMPILER}" CACHE FILEPATH
        "Compiler ranlib used by fishydslib when zig cc is selected" FORCE)
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> ar qc <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_APPEND "<CMAKE_AR> ar q <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> ranlib <TARGET>")
endif()

# Set C17 standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build options
option(FISHYDS_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(FISHYDS_ENABLE_TESTS "Build tests" ON)
option(FISHYDS_ENABLE_EXAMPLES "Build examples" OFF)
option(FISHYDS_ENABLE_BENCHMARKS "Build benchmarks" OFF)
option(FISHYDS_USE_GLIB_ALLOC "Use GLib allocators for dc_alloc" OFF)

include(CheckCCompilerFlag)

set(FISHYDS_C_COMPILER_FRONTEND "")
if(DEFINED CMAKE_C_COMPILER_FRONTEND_VARIANT)
    set(FISHYDS_C_COMPILER_FRONTEND "${CMAKE_C_COMPILER_FRONTEND_VARIANT}")
endif()

if(FISHYDS_DISCOVERED_C_COMPILERS)
    string(REPLACE ";" ", " _fishyds_discovered_c_compilers "${FISHYDS_DISCOVERED_C_COMPILERS}")
    message(STATUS "fishydslib detected C compiler candidates: ${_fishyds_discovered_c_compilers}")
endif()

message(STATUS
    "fishydslib selected C compiler: ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1} "
    "(${CMAKE_C_COMPILER_ID}, frontend: ${FISHYDS_C_COMPILER_FRONTEND})")

function(fishyds_append_supported_c_flags out_var)
    set(_fishyds_supported_flags "${${out_var}}")

    foreach(_flag IN LISTS ARGN)
        string(MAKE_C_IDENTIFIER "fishyds_has_${_flag}" _flag_cache_var)
        set(CMAKE_REQUIRED_QUIET TRUE)
        check_c_compiler_flag("${_flag}" "${_flag_cache_var}")
        if(${_flag_cache_var})
            list(APPEND _fishyds_supported_flags "${_flag}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _fishyds_supported_flags)
    set(${out_var} "${_fishyds_supported_flags}" PARENT_SCOPE)
endfunction()

# Compiler flags for hardening and warnings
set(FISHYDS_COMPILE_FLAGS "")
set(FISHYDS_LINK_FLAGS "")

if(FISHYDS_C_COMPILER_FRONTEND STREQUAL "GNU"
   OR CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang|IntelLLVM")
    fishyds_append_supported_c_flags(FISHYDS_COMPILE_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wformat=2
        -Wformat-security
        -Wnull-dereference
        -Wstack-protector
        -Wtrampolines
        -Walloca
        -Wvla
        -Warray-bounds=2
        -Wimplicit-fallthrough
        -Wtraditional-conversion
        -Wshift-overflow=2
        -Wcast-qual
        -Wstringop-overflow=4
        -Wconversion
        -Warith-conversion
        -Wlogical-op
        -Wduplicated-cond
        -Wduplicated-branches
        -Wformat-overflow=2
        -Wformat-truncation=2
        -Wstringop-truncation
        -fno-common
        -fstack-protector-strong
        -fPIC
    )

    if(FISHYDS_ENABLE_SANITIZERS)
        fishyds_append_supported_c_flags(FISHYDS_COMPILE_FLAGS
            -fsanitize=address
            -fsanitize=undefined
            -fno-sanitize-recover=all
            -fsanitize-address-use-after-scope
        )

        fishyds_append_supported_c_flags(FISHYDS_LINK_FLAGS
            -fsanitize=address
            -fsanitize=undefined
        )
    endif()
elseif(FISHYDS_C_COMPILER_FRONTEND STREQUAL "MSVC" OR MSVC)
    fishyds_append_supported_c_flags(FISHYDS_COMPILE_FLAGS
        /W4
        /sdl
    )

    if(FISHYDS_ENABLE_SANITIZERS)
        fishyds_append_supported_c_flags(FISHYDS_COMPILE_FLAGS /fsanitize=address)
        fishyds_append_supported_c_flags(FISHYDS_LINK_FLAGS /fsanitize=address)
    endif()
endif()

# Find dependencies
find_package(PkgConfig QUIET)

set(FISHYDS_DEP_LINK_LIBS "")
set(FISHYDS_DEP_INCLUDE_DIRS "")
set(FISHYDS_DEP_COMPILE_DEFINITIONS "")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/fishyds_dependencies.cmake")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/json)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/http)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/gw)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/model)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/client)

# Source files
set(FISHYDS_SOURCES
    # Core utilities
    core/dc_status.c
    core/dc_alloc.c
    core/dc_log.c
    core/dc_platform.c
    core/dc_allowed_mentions.c
    core/dc_attachments.c
    core/dc_cdn.c
    core/dc_data_uri.c
    core/dc_env.c
    core/dc_string.c
    core/dc_vec.c
    core/dc_snowflake.c
    core/dc_time.c
    core/dc_format.c

    # JSON layer
    json/dc_json.c
    json/dc_json_model.c
    json/dc_json_component.c

    # HTTP client
    http/dc_http.c
    http/dc_rest.c
    http/dc_http_compliance.c
    http/dc_multipart.c

    # Gateway client
    gw/dc_gateway.c
    gw/dc_events.c
    gw/dc_gateway_codes.c

    # Models
    model/dc_user.c
    model/dc_role.c
    model/dc_guild_member.c
    model/dc_channel.c
    model/dc_component.c
    model/dc_attachment.c
    model/dc_embed.c
    model/dc_message.c
    model/dc_interaction.c
    model/dc_guild.c
    model/dc_voice_state.c
    model/dc_presence.c
    model/dc_permissions.c

    # Client
    client/dc_client.c
    client/dc_commands.c
)

# Create static library
add_library(discordc STATIC ${FISHYDS_SOURCES})

# Apply compile flags
target_compile_options(discordc PRIVATE ${FISHYDS_COMPILE_FLAGS})
if(FISHYDS_ENABLE_SANITIZERS AND FISHYDS_LINK_FLAGS)
    target_link_options(discordc PRIVATE ${FISHYDS_LINK_FLAGS})
endif()
if(UNIX)
    target_compile_definitions(discordc PRIVATE _POSIX_C_SOURCE=200809L)
endif()
if(FISHYDS_USE_GLIB_ALLOC)
    target_compile_definitions(discordc PRIVATE DC_USE_GLIB_ALLOC=1)
endif()

# Link dependencies
target_link_libraries(discordc
    PRIVATE
    ${FISHYDS_DEP_LINK_LIBS}
)

# Include directories for dependencies
target_include_directories(discordc
    PRIVATE
    ${FISHYDS_DEP_INCLUDE_DIRS}
)

# Export includes
target_include_directories(discordc
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/fishydslib>
)

# Examples
if(FISHYDS_ENABLE_EXAMPLES)
    set(_ex_dir "${CMAKE_CURRENT_SOURCE_DIR}/examples")
    if(EXISTS "${_ex_dir}/CMakeLists.txt")
        add_subdirectory(examples)
    else()
        message(STATUS "Examples enabled, but '${_ex_dir}' is missing or has no CMakeLists.txt - skipping examples")
    endif()
endif()

# Tests
if(FISHYDS_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(FISHYDS_ENABLE_BENCHMARKS)
    enable_language(CXX)
    add_subdirectory(benchmarks)
endif()

# Install targets
install(TARGETS discordc
    EXPORT fishydslibTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# Install headers
install(DIRECTORY core/ json/ http/ gw/ model/ client/
    DESTINATION include/fishydslib
    FILES_MATCHING PATTERN "*.h"
)

# Export configuration
install(EXPORT fishydslibTargets
    FILE fishydslibTargets.cmake
    NAMESPACE fishydslib::
    DESTINATION lib/cmake/fishydslib
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    fishydslibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/fishydslibConfigVersion.cmake
    DESTINATION lib/cmake/fishydslib
)
