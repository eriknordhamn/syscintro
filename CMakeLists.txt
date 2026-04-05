cmake_minimum_required(VERSION 3.16)

project(mesh_network)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SystemC - checks SYSTEMC_HOME env variable first, then common paths
if(DEFINED ENV{SYSTEMC_HOME})
    set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
elseif(EXISTS /opt/systemc)
    set(SYSTEMC_HOME /opt/systemc)
elseif(EXISTS /usr/local/systemc)
    set(SYSTEMC_HOME /usr/local/systemc)
endif()

if(SYSTEMC_HOME)
    message(STATUS "Using SystemC from: ${SYSTEMC_HOME}")

    find_library(SYSTEMC_LIBRARY
        NAMES systemc
        PATHS ${SYSTEMC_HOME}/lib ${SYSTEMC_HOME}/lib-linux64
        NO_DEFAULT_PATH
    )

    set(SYSTEMC_INCLUDE_DIR ${SYSTEMC_HOME}/include)
else()
    message(FATAL_ERROR
        "SystemC not found. Set SYSTEMC_HOME environment variable.\n"
        "  export SYSTEMC_HOME=/path/to/systemc\n"
        "See README.md for installation instructions."
    )
endif()

add_executable(mesh_sim src/main.cpp)

target_include_directories(mesh_sim PRIVATE
    ${SYSTEMC_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(mesh_sim ${SYSTEMC_LIBRARY})
