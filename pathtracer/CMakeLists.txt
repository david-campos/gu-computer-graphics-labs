cmake_minimum_required ( VERSION 3.0.2 )

set(CMAKE_VERBOSE_MAKEFILE  on)

project ( pathtracer )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package ( embree 2.12 REQUIRED )
include_directories ( ${EMBREE_INCLUDE_DIRS} )
message(${EMBREE_LIBRARIES})

find_package ( OpenMP REQUIRED )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

# Find *all* shaders.
file(GLOB_RECURSE SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
)
# Separate filter for shaders.
source_group("Shaders" FILES ${SHADERS})

# Build and link executable.
add_executable ( ${PROJECT_NAME}
    main.cpp
    Pathtracer.h
    Pathtracer.cpp
    sampling.h
    sampling.cpp
    HDRImage.h
    HDRImage.cpp
    embree.h
    embree.cpp
    material.h
    material.cpp
    ${SHADERS}
        light.h geometry.h aux.h)

target_link_libraries ( ${PROJECT_NAME} labhelper ${EMBREE_LIBRARIES} )
config_build_output()
