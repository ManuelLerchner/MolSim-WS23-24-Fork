cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# define project name, version
project(PSEMolDyn_GroupC VERSION 0.0.1)

# let ccmake and cmake-gui offer the default build type options
set_property(CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS "Debug;Release;RelWithDebInfo;MinSizeRel")

# set Release as the default build type if it is not yet set.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE
            "Release"
            CACHE
            STRING
            "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
            FORCE)

endif(NOT CMAKE_BUILD_TYPE)

add_compile_options(-Ofast -march=native)

# modules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/)

# project dependencies
include(boost_program_options)
include(libxsd)
include(spdlog)
include(xercesc)

# tests and documentation
include(gtest) #provides a BUILD_TESTS option
include(doxygen) #provides a BUILD_DOC_DOXYGEN option

# convenience target to format all source files
include(clang-format) #provides a clangformat target

# add subdirectories
add_subdirectory(src project)

add_subdirectory(tests)

add_subdirectory(benchmarks)
