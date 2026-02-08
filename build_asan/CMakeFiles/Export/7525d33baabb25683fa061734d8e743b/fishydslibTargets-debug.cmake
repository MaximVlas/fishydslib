#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "fishydslib::discordc" for configuration "Debug"
set_property(TARGET fishydslib::discordc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(fishydslib::discordc PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libdiscordc.a"
  )

list(APPEND _cmake_import_check_targets fishydslib::discordc )
list(APPEND _cmake_import_check_files_for_fishydslib::discordc "${_IMPORT_PREFIX}/lib/libdiscordc.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
