cmake_minimum_required(VERSION 3.16)
project(fishydslib VERSION 0.1.0 LANGUAGES C)

# Set C17 standard
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build options
option(FISHYDS_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(FISHYDS_ENABLE_TESTS "Build tests" ON)
option(FISHYDS_ENABLE_EXAMPLES "Build examples" ON)
option(FISHYDS_ENABLE_BENCHMARKS "Build benchmarks" OFF)
option(FISHYDS_USE_GLIB_ALLOC "Use GLib allocators for dc_alloc" ON)

# Compiler flags for hardening and warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(FISHYDS_COMPILE_FLAGS
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
        list(APPEND FISHYDS_COMPILE_FLAGS
            -fsanitize=address
            -fsanitize=undefined
            -fno-sanitize-recover=all
            -fsanitize-address-use-after-scope
        )
        set(FISHYDS_LINK_FLAGS
            -fsanitize=address
            -fsanitize=undefined
        )
    endif()
endif()

# Find dependencies
find_package(PkgConfig REQUIRED)

# libcurl
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "libcurl not found")
endif()

# libwebsockets
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)
if(NOT LIBWEBSOCKETS_FOUND)
    message(FATAL_ERROR "libwebsockets not found")
endif()

# yyjson
pkg_check_modules(YYJSON REQUIRED yyjson)
if(NOT YYJSON_FOUND)
    message(FATAL_ERROR "yyjson not found")
endif()

# zlib
find_package(ZLIB REQUIRED)
if(NOT ZLIB_FOUND)
    message(FATAL_ERROR "zlib not found")
endif()

# glib (optional)
if(FISHYDS_USE_GLIB_ALLOC)
    pkg_check_modules(GLIB REQUIRED glib-2.0)
    if(NOT GLIB_FOUND)
        message(FATAL_ERROR "glib-2.0 not found")
    endif()
endif()

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
    model/dc_message.c
    model/dc_guild.c
    
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
    ${CURL_LIBRARIES}
    ${LIBWEBSOCKETS_LIBRARIES}
    ${YYJSON_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${GLIB_LIBRARIES}
)

# Include directories for dependencies
target_include_directories(discordc
    PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ${YYJSON_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

# Export includes
target_include_directories(discordc
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/fishydslib>
)

# Examples
if(FISHYDS_ENABLE_EXAMPLES)
    add_subdirectory(examples)
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
