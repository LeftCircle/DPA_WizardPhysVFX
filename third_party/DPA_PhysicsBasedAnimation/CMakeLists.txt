cmake_minimum_required(VERSION 3.24)

project(PhysicsBasedAnimation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenMP REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(TBB REQUIRED)

file(GLOB BASE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/base/*.C"
)

file(GLOB THINGS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/things/src/*.cpp"
)

list(FILTER THINGS_SOURCES EXCLUDE REGEX ".*/pbalitesim\\.cpp$")

add_library(pba STATIC
    ${BASE_SOURCES}
    ${THINGS_SOURCES}
)

target_include_directories(pba
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/things/include"
)

target_compile_options(pba
    PRIVATE
        -Wall
        -g
        -O2
        -fPIC
)

target_link_libraries(pba
    PUBLIC
        OpenMP::OpenMP_CXX
        OpenGL::GL
        OpenGL::GLU
        GLUT::GLUT
        TBB::tbb
        ${CMAKE_DL_LIBS}
        m
)

add_executable(pbalitesim
    things/src/pbalitesim.cpp
)

target_link_libraries(pbalitesim PRIVATE pba)

# set_target_properties(pbalitesim PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
# )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

option(PBA_BUILD_TESTS "Build PhysicsBasedAnimation tests" ON)

if(PBA_BUILD_TESTS)
    add_executable(test_runner
        ${TEST_SOURCES}
        tests/catch2/catch_amalgamated.cpp
    )

    target_include_directories(test_runner
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/catch2"
    )

    target_link_libraries(test_runner PRIVATE pba)

    enable_testing()
    add_test(NAME physics_tests COMMAND test_runner)
endif()