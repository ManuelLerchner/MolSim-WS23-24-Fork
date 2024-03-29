include_directories(
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE BENCHMARK_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h")


add_custom_target(benchmarks DEPENDS ${BENCHMARK_SRC})

# add executable for each benchmark
foreach (_source IN ITEMS ${BENCHMARK_SRC})
    if (${_source} MATCHES ".*\\.h$")
        continue()
    endif ()
    get_filename_component(_name ${_source} NAME_WE)
    add_executable(${_name} EXCLUDE_FROM_ALL ${_source})
    target_link_libraries(${_name} project_lib)
    add_dependencies(benchmarks ${_name})
    target_compile_definitions(${_name} PRIVATE BENCHMARK_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
endforeach ()
