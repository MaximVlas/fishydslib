# Benchmarks CMakeLists.txt

find_package(benchmark REQUIRED)

# Debug benchmarks (current behavior)
add_executable(bench_core
    bench_core.cc
)

add_executable(bench_http
    bench_http.cc
)

add_executable(bench_json
    bench_json.cc
)

add_executable(bench_gateway
    bench_gateway.cc
)

add_executable(bench_format
    bench_format.cc
)

add_executable(bench_cdn
    bench_cdn.cc
)

add_executable(bench_multipart
    bench_multipart.cc
)

# Optimized benchmarks (Release build with -O3)
add_executable(bench_core_opt
    bench_core.cc
)

add_executable(bench_http_opt
    bench_http.cc
)

add_executable(bench_json_opt
    bench_json.cc
)

add_executable(bench_gateway_opt
    bench_gateway.cc
)

add_executable(bench_format_opt
    bench_format.cc
)

add_executable(bench_cdn_opt
    bench_cdn.cc
)

add_executable(bench_multipart_opt
    bench_multipart.cc
)

# Link debug benchmarks
target_link_libraries(bench_core
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_http
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_json
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_gateway
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_format
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_cdn
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_multipart
    PRIVATE
    discordc
    benchmark::benchmark
)

# Link optimized benchmarks
target_link_libraries(bench_core_opt
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_http_opt
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_json_opt
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_gateway_opt
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_format_opt
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_cdn_opt
    PRIVATE
    discordc
    benchmark::benchmark
)

target_link_libraries(bench_multipart_opt
    PRIVATE
    discordc
    benchmark::benchmark
)

# Debug benchmark compile options
target_compile_options(bench_core PRIVATE ${FISHYDS_COMPILE_FLAGS})
target_compile_options(bench_http PRIVATE ${FISHYDS_COMPILE_FLAGS})
target_compile_options(bench_json PRIVATE ${FISHYDS_COMPILE_FLAGS})
target_compile_options(bench_gateway PRIVATE ${FISHYDS_COMPILE_FLAGS})
target_compile_options(bench_format PRIVATE ${FISHYDS_COMPILE_FLAGS})
target_compile_options(bench_cdn PRIVATE ${FISHYDS_COMPILE_FLAGS})
target_compile_options(bench_multipart PRIVATE ${FISHYDS_COMPILE_FLAGS})

# Optimized benchmark compile options
target_compile_options(bench_core_opt PRIVATE -O3 -DNDEBUG -march=native -flto)
target_compile_options(bench_http_opt PRIVATE -O3 -DNDEBUG -march=native -flto)
target_compile_options(bench_json_opt PRIVATE -O3 -DNDEBUG -march=native -flto)
target_compile_options(bench_gateway_opt PRIVATE -O3 -DNDEBUG -march=native -flto)
target_compile_options(bench_format_opt PRIVATE -O3 -DNDEBUG -march=native -flto)
target_compile_options(bench_cdn_opt PRIVATE -O3 -DNDEBUG -march=native -flto)
target_compile_options(bench_multipart_opt PRIVATE -O3 -DNDEBUG -march=native -flto)

# Set C++ standard
set_target_properties(bench_core bench_core_opt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set_target_properties(bench_http bench_http_opt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set_target_properties(bench_json bench_json_opt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set_target_properties(bench_gateway bench_gateway_opt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set_target_properties(bench_format bench_format_opt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set_target_properties(bench_cdn bench_cdn_opt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
set_target_properties(bench_multipart bench_multipart_opt PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

# Link options for optimized builds
target_link_options(bench_core_opt PRIVATE -O3 -flto)
target_link_options(bench_http_opt PRIVATE -O3 -flto)
target_link_options(bench_json_opt PRIVATE -O3 -flto)
target_link_options(bench_gateway_opt PRIVATE -O3 -flto)
target_link_options(bench_format_opt PRIVATE -O3 -flto)
target_link_options(bench_cdn_opt PRIVATE -O3 -flto)
target_link_options(bench_multipart_opt PRIVATE -O3 -flto)

if(FISHYDS_ENABLE_SANITIZERS AND FISHYDS_LINK_FLAGS)
    target_link_options(bench_core PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(bench_http PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(bench_json PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(bench_gateway PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(bench_format PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(bench_cdn PRIVATE ${FISHYDS_LINK_FLAGS})
    target_link_options(bench_multipart PRIVATE ${FISHYDS_LINK_FLAGS})
endif()
