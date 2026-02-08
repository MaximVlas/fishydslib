# Tests CMakeLists.txt

# Test utilities
add_library(test_utils STATIC test_utils.c)
target_link_libraries(test_utils discordc)
target_compile_options(test_utils PRIVATE ${FISHYDS_COMPILE_FLAGS})

# Core tests
add_executable(test_core 
    test_status.c
    test_alloc.c
    test_string.c
    test_vec.c
    test_snowflake.c
    test_time.c
    test_optional.c
    test_format.c
    test_allowed_mentions.c
    test_cdn.c
    test_data_uri.c
    test_attachments.c
    test_core_main.c
)
target_link_libraries(test_core discordc test_utils)
target_compile_options(test_core PRIVATE ${FISHYDS_COMPILE_FLAGS})

# JSON tests
add_executable(test_json
    test_json.c
    test_json_main.c
)
target_link_libraries(test_json discordc test_utils)
target_compile_options(test_json PRIVATE ${FISHYDS_COMPILE_FLAGS})

# HTTP tests
add_executable(test_http
    test_http.c
    test_multipart.c
    test_http_main.c
)
target_link_libraries(test_http discordc test_utils)
target_compile_options(test_http PRIVATE ${FISHYDS_COMPILE_FLAGS})

# REST tests
add_executable(test_rest
    test_rest.c
    test_rest_main.c
)
target_link_libraries(test_rest discordc test_utils)
target_compile_options(test_rest PRIVATE ${FISHYDS_COMPILE_FLAGS})

# Gateway tests
add_executable(test_gateway
    test_gateway.c
    test_gateway_main.c
)
target_link_libraries(test_gateway discordc test_utils)
target_compile_options(test_gateway PRIVATE ${FISHYDS_COMPILE_FLAGS})

# Apply sanitizers to tests if enabled
if(FISHYDS_ENABLE_SANITIZERS AND FISHYDS_LINK_FLAGS)
    target_link_options(test_utils PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(test_core PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(test_json PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(test_http PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(test_rest PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(test_gateway PRIVATE ${FISHYDS_LINK_FLAGS})
endif()

# Add tests to CTest
add_test(NAME core_tests COMMAND test_core)
add_test(NAME json_tests COMMAND test_json)
add_test(NAME http_tests COMMAND test_http)
add_test(NAME rest_tests COMMAND test_rest)
add_test(NAME gateway_tests COMMAND test_gateway)

# Install tests
install(TARGETS test_core test_json test_http test_rest test_gateway
    RUNTIME DESTINATION bin/tests
)
