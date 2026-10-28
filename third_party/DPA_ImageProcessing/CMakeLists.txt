cmake_minimum_required(VERSION 3.22)

project(ImageProcessing LANGUAGES CXX)

option(IMAGE_PROCESSING_BUILD_VIEWER "Build imgviewer" OFF)
option(IMAGE_PROCESSING_BUILD_TESTS "Build ImageProcessing tests" OFF)
option(IMAGE_PROCESSING_BUILD_PYTHON "Build SWIG Python bindings" OFF)

set(IMAGE_PROCESSING_DEPS_ROOT "" CACHE PATH
    "Prefix containing include/ and lib/ for OpenImageIO/OpenEXR"
)

set(IP_INCLUDE_HINTS)
set(IP_LIBRARY_HINTS)

if(IMAGE_PROCESSING_DEPS_ROOT)
    list(APPEND IP_INCLUDE_HINTS
        "${IMAGE_PROCESSING_DEPS_ROOT}/include"
    )
    list(APPEND IP_LIBRARY_HINTS
        "${IMAGE_PROCESSING_DEPS_ROOT}/lib"
    )
endif()

if(IMAGE_PROCESSING_DEPS_ROOT)
    list(APPEND CMAKE_BUILD_RPATH
        "${IMAGE_PROCESSING_DEPS_ROOT}/lib"
    )
endif()

find_path(IMAGE_PROCESSING_OIIO_INCLUDE_DIR
    NAMES OpenImageIO/imageio.h
    HINTS ${IP_INCLUDE_HINTS}
    REQUIRED
)

find_path(IMAGE_PROCESSING_OPENEXR_INCLUDE_DIR
    NAMES OpenEXR/ImfHeader.h
    HINTS ${IP_INCLUDE_HINTS}
    REQUIRED
)

find_library(IMAGE_PROCESSING_OIIO_LIBRARY
    NAMES OpenImageIO
    HINTS ${IP_LIBRARY_HINTS}
    REQUIRED
)

find_library(IMAGE_PROCESSING_OIIO_UTIL_LIBRARY
    NAMES OpenImageIO_Util
    HINTS ${IP_LIBRARY_HINTS}
    REQUIRED
)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(OpenMP REQUIRED)

file(GLOB_RECURSE IMAGE_PROCESSING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Do not put executable/test entry points into the library.
list(FILTER IMAGE_PROCESSING_SOURCES
    EXCLUDE REGEX "/(main|tests)\\.cpp$"
)

add_library(image_processing STATIC
    ${IMAGE_PROCESSING_SOURCES}
)

set_target_properties(image_processing PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_compile_features(image_processing PUBLIC cxx_std_17)

target_compile_options(image_processing PRIVATE
    -Wall
)

target_include_directories(image_processing PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${IMAGE_PROCESSING_OIIO_INCLUDE_DIR}"
    "${IMAGE_PROCESSING_OPENEXR_INCLUDE_DIR}"
)

target_link_libraries(image_processing PUBLIC
    "${IMAGE_PROCESSING_OIIO_LIBRARY}"
    "${IMAGE_PROCESSING_OIIO_UTIL_LIBRARY}"
    OpenGL::GL
    OpenGL::GLU
    GLUT::GLUT
    OpenMP::OpenMP_CXX
)

if(IMAGE_PROCESSING_BUILD_VIEWER)
    add_executable(dpa_imgviewer
        "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
    )

    target_link_libraries(dpa_imgviewer PRIVATE
        image_processing
    )
endif()