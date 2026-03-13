include_guard(GLOBAL)

include(FetchContent)

option(FISHYDS_FETCH_MISSING_DEPS
    "Automatically fetch supported missing dependencies from release archives"
    ON)

if(WIN32)
    string(CONCAT _fishyds_windows_dependency_hint
        " On Windows, prefer the bundled vcpkg manifest by configuring from a clean build directory with "
        "-DCMAKE_TOOLCHAIN_FILE=\"$ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake\" so vcpkg installs "
        "curl, libwebsockets, yyjson, and zlib automatically.")
else()
    set(_fishyds_windows_dependency_hint "")
endif()

set(_fishyds_vcpkg_manifest_hint "")
if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json"
   AND DEFINED CMAKE_TOOLCHAIN_FILE
   AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg[.]cmake"
   AND DEFINED VCPKG_MANIFEST_MODE
   AND NOT VCPKG_MANIFEST_MODE)
    string(CONCAT _fishyds_vcpkg_manifest_hint
        " This build directory is currently using vcpkg classic mode (VCPKG_MANIFEST_MODE=OFF), "
        "so it will ignore the repository's vcpkg.json manifest. Delete the build directory or at least "
        "remove CMakeCache.txt and re-run CMake so manifest mode can activate.")
endif()

macro(fishyds_find_first_target out_var)
    set(${out_var} "")
    foreach(_fishyds_candidate ${ARGN})
        if(TARGET ${_fishyds_candidate})
            set(${out_var} ${_fishyds_candidate})
            break()
        endif()
    endforeach()
endmacro()

# libcurl
find_package(CURL QUIET)
if(TARGET CURL::libcurl)
    list(APPEND FISHYDS_DEP_LINK_LIBS CURL::libcurl)
elseif(CURL_FOUND)
    list(APPEND FISHYDS_DEP_LINK_LIBS ${CURL_LIBRARIES})
    list(APPEND FISHYDS_DEP_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
else()
    message(FATAL_ERROR
        "libcurl not found. Install a CMake package/config for libcurl or provide CURL_LIBRARY and "
        "CURL_INCLUDE_DIR.${_fishyds_windows_dependency_hint}${_fishyds_vcpkg_manifest_hint}")
endif()

# libwebsockets
set(FISHYDS_LIBWEBSOCKETS_FOUND FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBWEBSOCKETS QUIET IMPORTED_TARGET libwebsockets)
    if(TARGET PkgConfig::LIBWEBSOCKETS)
        list(APPEND FISHYDS_DEP_LINK_LIBS PkgConfig::LIBWEBSOCKETS)
        set(FISHYDS_LIBWEBSOCKETS_FOUND TRUE)
    endif()
endif()
if(NOT FISHYDS_LIBWEBSOCKETS_FOUND)
    find_package(libwebsockets CONFIG QUIET)
    fishyds_find_first_target(FISHYDS_LIBWEBSOCKETS_TARGET
        websockets
        libwebsockets
        libwebsockets::websockets
        unofficial-libwebsockets::libwebsockets)
    if(FISHYDS_LIBWEBSOCKETS_TARGET)
        list(APPEND FISHYDS_DEP_LINK_LIBS ${FISHYDS_LIBWEBSOCKETS_TARGET})
        set(FISHYDS_LIBWEBSOCKETS_FOUND TRUE)
    endif()
endif()
if(NOT FISHYDS_LIBWEBSOCKETS_FOUND)
    find_path(FISHYDS_LIBWEBSOCKETS_INCLUDE_DIR NAMES libwebsockets.h)
    find_library(FISHYDS_LIBWEBSOCKETS_LIBRARY NAMES websockets libwebsockets)
    if(FISHYDS_LIBWEBSOCKETS_INCLUDE_DIR AND FISHYDS_LIBWEBSOCKETS_LIBRARY)
        list(APPEND FISHYDS_DEP_LINK_LIBS ${FISHYDS_LIBWEBSOCKETS_LIBRARY})
        list(APPEND FISHYDS_DEP_INCLUDE_DIRS ${FISHYDS_LIBWEBSOCKETS_INCLUDE_DIR})
        set(FISHYDS_LIBWEBSOCKETS_FOUND TRUE)
    endif()
endif()
if(NOT FISHYDS_LIBWEBSOCKETS_FOUND)
    message(FATAL_ERROR
        "libwebsockets not found. Install a CMake package/config for libwebsockets, provide pkg-config "
        "metadata, or set FISHYDS_LIBWEBSOCKETS_INCLUDE_DIR and FISHYDS_LIBWEBSOCKETS_LIBRARY."
        "${_fishyds_windows_dependency_hint}${_fishyds_vcpkg_manifest_hint}")
endif()

# yyjson
set(FISHYDS_YYJSON_FOUND FALSE)
if(PkgConfig_FOUND)
    pkg_check_modules(YYJSON QUIET IMPORTED_TARGET yyjson)
    if(TARGET PkgConfig::YYJSON)
        list(APPEND FISHYDS_DEP_LINK_LIBS PkgConfig::YYJSON)
        set(FISHYDS_YYJSON_FOUND TRUE)
    endif()
endif()
if(NOT FISHYDS_YYJSON_FOUND)
    find_package(yyjson CONFIG QUIET)
    fishyds_find_first_target(FISHYDS_YYJSON_TARGET
        yyjson
        yyjson::yyjson
        unofficial::yyjson::yyjson)
    if(FISHYDS_YYJSON_TARGET)
        list(APPEND FISHYDS_DEP_LINK_LIBS ${FISHYDS_YYJSON_TARGET})
        set(FISHYDS_YYJSON_FOUND TRUE)
    endif()
endif()
if(NOT FISHYDS_YYJSON_FOUND)
    find_path(FISHYDS_YYJSON_INCLUDE_DIR NAMES yyjson.h)
    find_library(FISHYDS_YYJSON_LIBRARY NAMES yyjson)
    if(FISHYDS_YYJSON_INCLUDE_DIR AND FISHYDS_YYJSON_LIBRARY)
        list(APPEND FISHYDS_DEP_LINK_LIBS ${FISHYDS_YYJSON_LIBRARY})
        list(APPEND FISHYDS_DEP_INCLUDE_DIRS ${FISHYDS_YYJSON_INCLUDE_DIR})
        set(FISHYDS_YYJSON_FOUND TRUE)
    endif()
endif()
if(NOT FISHYDS_YYJSON_FOUND AND FISHYDS_FETCH_MISSING_DEPS)
    message(STATUS "yyjson not found locally; fetching release archive")
    set(YYJSON_BUILD_TESTS OFF CACHE BOOL "Build yyjson tests" FORCE)
    set(YYJSON_BUILD_FUZZER OFF CACHE BOOL "Build yyjson fuzzers" FORCE)
    set(YYJSON_BUILD_MISC OFF CACHE BOOL "Build yyjson misc targets" FORCE)
    set(YYJSON_BUILD_DOC OFF CACHE BOOL "Build yyjson docs" FORCE)
    set(YYJSON_INSTALL OFF CACHE BOOL "Install yyjson targets" FORCE)
    FetchContent_Declare(
        fishyds_yyjson
        URL
            "https://github.com/ibireme/yyjson/archive/refs/tags/0.12.0.tar.gz"
            "https://codeload.github.com/ibireme/yyjson/tar.gz/refs/tags/0.12.0"
    )
    FetchContent_MakeAvailable(fishyds_yyjson)
    fishyds_find_first_target(FISHYDS_YYJSON_FETCHED_TARGET
        yyjson
        yyjson::yyjson
        unofficial::yyjson::yyjson)
    if(FISHYDS_YYJSON_FETCHED_TARGET)
        list(APPEND FISHYDS_DEP_LINK_LIBS ${FISHYDS_YYJSON_FETCHED_TARGET})
        set(FISHYDS_YYJSON_FOUND TRUE)
    endif()
endif()
if(NOT FISHYDS_YYJSON_FOUND)
    message(FATAL_ERROR
        "yyjson not found. Install a CMake package/config for yyjson, provide pkg-config metadata, "
        "set FISHYDS_YYJSON_INCLUDE_DIR and FISHYDS_YYJSON_LIBRARY, or leave "
        "FISHYDS_FETCH_MISSING_DEPS=ON so fishydslib can download the release archive."
        "${_fishyds_vcpkg_manifest_hint}")
endif()

# zlib
find_package(ZLIB QUIET)
if(TARGET ZLIB::ZLIB)
    list(APPEND FISHYDS_DEP_LINK_LIBS ZLIB::ZLIB)
elseif(ZLIB_FOUND)
    list(APPEND FISHYDS_DEP_LINK_LIBS ${ZLIB_LIBRARIES})
    list(APPEND FISHYDS_DEP_INCLUDE_DIRS ${ZLIB_INCLUDE_DIRS})
elseif(FISHYDS_FETCH_MISSING_DEPS)
    message(STATUS "zlib not found locally; fetching release archive")
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Build zlib examples" FORCE)
    FetchContent_Declare(
        fishyds_zlib
        URL
            "https://www.zlib.net/zlib-1.3.1.tar.gz"
            "https://zlib.net/fossils/zlib-1.3.1.tar.gz"
    )
    FetchContent_MakeAvailable(fishyds_zlib)
    fishyds_find_first_target(FISHYDS_ZLIB_FETCHED_TARGET
        ZLIB::ZLIB
        zlibstatic
        zlib)
    if(FISHYDS_ZLIB_FETCHED_TARGET)
        list(APPEND FISHYDS_DEP_LINK_LIBS ${FISHYDS_ZLIB_FETCHED_TARGET})
        set(ZLIB_FOUND TRUE)
    endif()
endif()
if(NOT ZLIB_FOUND)
    message(FATAL_ERROR
        "zlib not found. Install a CMake package/config for zlib or leave FISHYDS_FETCH_MISSING_DEPS=ON "
        "so fishydslib can download the release archive.${_fishyds_vcpkg_manifest_hint}")
endif()

# glib (optional)
if(FISHYDS_USE_GLIB_ALLOC)
    set(FISHYDS_GLIB_FOUND FALSE)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLIB QUIET IMPORTED_TARGET glib-2.0)
        if(TARGET PkgConfig::GLIB)
            list(APPEND FISHYDS_DEP_LINK_LIBS PkgConfig::GLIB)
            set(FISHYDS_GLIB_FOUND TRUE)
        endif()
    endif()
    if(NOT FISHYDS_GLIB_FOUND)
        find_path(FISHYDS_GLIB_INCLUDE_DIR NAMES glib.h PATH_SUFFIXES glib-2.0)
        find_path(FISHYDS_GLIB_CONFIG_INCLUDE_DIR NAMES glibconfig.h PATH_SUFFIXES lib/glib-2.0/include glib-2.0/include)
        find_library(FISHYDS_GLIB_LIBRARY NAMES glib-2.0 glib-2.0-0 libglib-2.0 libglib-2.0-0)
        if(FISHYDS_GLIB_INCLUDE_DIR AND FISHYDS_GLIB_LIBRARY)
            list(APPEND FISHYDS_DEP_LINK_LIBS ${FISHYDS_GLIB_LIBRARY})
            list(APPEND FISHYDS_DEP_INCLUDE_DIRS ${FISHYDS_GLIB_INCLUDE_DIR} ${FISHYDS_GLIB_CONFIG_INCLUDE_DIR})
            set(FISHYDS_GLIB_FOUND TRUE)
        endif()
    endif()
    if(NOT FISHYDS_GLIB_FOUND)
        message(FATAL_ERROR
            "glib-2.0 not found. Install a CMake/package-manager build of GLib, provide pkg-config metadata, "
            "or set FISHYDS_GLIB_INCLUDE_DIR, FISHYDS_GLIB_CONFIG_INCLUDE_DIR, and FISHYDS_GLIB_LIBRARY.")
    endif()
endif()
