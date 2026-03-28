add_executable(bench_small_obj bench_small_obj.cpp)
target_link_libraries(bench_small_obj PRIVATE modern_loki)

if(MSVC)
    target_compile_options(bench_small_obj PRIVATE /W4 /permissive- /O2)
else()
    target_compile_options(bench_small_obj PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()
