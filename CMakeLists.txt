cmake_minimum_required(VERSION 3.16)

project(mesh_network)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SystemC - checks SYSTEMC_HOME env variable first, then common paths
if(DEFINED ENV{SYSTEMC_HOME})
    set(SYSTEMC_HOME $ENV{SYSTEMC_HOME})
elseif(EXISTS /opt/systemc-3.0.2)
    set(SYSTEMC_HOME /opt/systemc-3.0.2)
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

# Mesh network simulation
add_executable(mesh_sim src/main.cpp)

target_include_directories(mesh_sim PRIVATE
    ${SYSTEMC_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(mesh_sim ${SYSTEMC_LIBRARY})

# Basic counter example
add_executable(counter src/counter.cpp)

target_include_directories(counter PRIVATE
    ${SYSTEMC_INCLUDE_DIR}
)

target_link_libraries(counter ${SYSTEMC_LIBRARY})

# TLM memory example (CPU + 256-byte memory over TLM-2.0)
add_executable(tlm_memory src/tlm_memory.cpp)

target_include_directories(tlm_memory PRIVATE
    ${SYSTEMC_INCLUDE_DIR}
)

target_link_libraries(tlm_memory ${SYSTEMC_LIBRARY})
