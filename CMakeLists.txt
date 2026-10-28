cmake_minimum_required(VERSION 3.24)

project(PhysicsBasedVFX
    VERSION 0.1.0
    LANGUAGES CXX CUDA
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)



# ------------------------------------------------------------
# CUDA
# ------------------------------------------------------------
# Choose the architecture appropriate for your GPU.
set(CMAKE_CUDA_ARCHITECTURES native)

find_package(CUDAToolkit REQUIRED)

# ------------------------------------------------------------
# Options
# ------------------------------------------------------------
option(PBVFX_BUILD_PYTHON "Build Python bindings" ON)


# Optional: build tests by default
include(CTest)

# ------------------------------------------------------------
# Starter library
# ------------------------------------------------------------

file(GLOB_RECURSE STARTER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/starter/base/*.C"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/starter/base/*.cpp"
)

add_library(starter STATIC ${STARTER_SOURCES})

set_target_properties(starter PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(starter
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/starter/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/starter/base"
)


# ------------------------------------------------------------
# Image Processing
# ------------------------------------------------------------

set(IMAGE_PROCESSING_DEPS_ROOT
    "/home/left/local"
    #"/home/leftcircle/programming"
    CACHE PATH
    "Prefix containing include/ and lib/ for OpenImageIO/OpenEXR"
)

set(IMAGE_PROCESSING_BUILD_VIEWER ON CACHE BOOL "" FORCE)
set(IMAGE_PROCESSING_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(IMAGE_PROCESSING_BUILD_PYTHON OFF CACHE BOOL "" FORCE)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/DPA_ImageProcessing"
    "${CMAKE_CURRENT_BINARY_DIR}/DPA_ImageProcessing"
)


# ------------------------------------------------------------
# Physics Based Animation!
# ------------------------------------------------------------

set(PBA_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/DPA_PhysicsBasedAnimation"
    "${CMAKE_CURRENT_BINARY_DIR}/DPA_PhysicsBasedAnimation"
)

# ------------------------------------------------------------
# OpenVDB
# ------------------------------------------------------------

set(OpenVDB_ROOT
    "/home/left/local"
    CACHE PATH
    "OpenVDB installation prefix"
)

# OpenVDB installs FindOpenVDB.cmake here.
list(PREPEND CMAKE_MODULE_PATH
    "${OpenVDB_ROOT}/lib/cmake/OpenVDB"
    "${OpenVDB_ROOT}/lib64/cmake/OpenVDB"
)

list(PREPEND CMAKE_PREFIX_PATH
    "${OpenVDB_ROOT}"
)


# ------------------------------------------------------------
# All my things!
# ------------------------------------------------------------
find_package(OpenVDB REQUIRED)
find_package(Boost REQUIRED)

file(GLOB_RECURSE PBVFX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.C"
)

file(GLOB_RECURSE PBVFX_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

add_library(pbvfx STATIC
    ${PBVFX_SOURCES}
    ${PBVFX_HEADERS}
)

target_sources(pbvfx PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cuda/ray_marcher_cuda.cu"
)

set_target_properties(pbvfx PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
)

target_include_directories(pbvfx
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/header_only"
)

target_link_libraries(pbvfx
    PUBLIC
        starter
        pba
        image_processing
        OpenVDB::openvdb
        Boost::headers
        CUDA::cudart
)

target_compile_definitions(pbvfx
    PUBLIC
        NANOVDB_USE_OPENVDB=1
)

target_compile_options(pbvfx PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--expt-relaxed-constexpr>
    $<$<COMPILE_LANGUAGE:CUDA>:--expt-extended-lambda>
)

# ------------------------------------------------------------
# Example/demo executable
# ------------------------------------------------------------

add_executable(physics_vfx
    main.cpp
)

target_link_libraries(physics_vfx
    PRIVATE
        pbvfx
)

# ------------------------------------------------------------
# Catch2 tests
# ------------------------------------------------------------

if(BUILD_TESTING)

    include(FetchContent)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
    )

    # Prevent Catch2's own tests from being built.
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(Catch2)

    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    add_executable(pbvfx_tests
        ${TEST_SOURCES}
    )

    target_compile_definitions(pbvfx_tests PRIVATE
        PBVFX_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    target_link_libraries(pbvfx_tests
        PRIVATE
            pbvfx
            Catch2::Catch2WithMain
    )

    # Catch2's CMake test-discovery helper is in its extras directory.
    list(APPEND CMAKE_MODULE_PATH
        ${catch2_SOURCE_DIR}/extras
    )

    include(Catch)

    catch_discover_tests(pbvfx_tests)

endif()

# ------------------------------------------------------------
# Python bindings
# ------------------------------------------------------------
if(PBVFX_BUILD_PYTHON)
    find_package(Python COMPONENTS Interpreter Development REQUIRED)

    include(FetchContent)

    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.13.6
    )

    FetchContent_MakeAvailable(pybind11)

    pybind11_add_module(physics_vfx_python
        "${CMAKE_CURRENT_SOURCE_DIR}/bindings/pybindings.cpp"
    )

    set_target_properties(physics_vfx_python PROPERTIES
        OUTPUT_NAME physics_vfx
        LIBRARY_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/python"
    )

    target_link_libraries(physics_vfx_python
        PRIVATE
            pbvfx
    )

    if(BUILD_TESTING)
        add_test(NAME physics_vfx_python_import
            COMMAND "${CMAKE_COMMAND}" -E env
                "PYTHONPATH=$<TARGET_FILE_DIR:physics_vfx_python>"
                "${Python_EXECUTABLE}" -c "import physics_vfx"
        )
    endif()
endif()
