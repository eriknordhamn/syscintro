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

# --- Learning demos ---
# Helper to add a demo executable
function(add_demo name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${SYSTEMC_INCLUDE_DIR})
    target_link_libraries(${name} ${SYSTEMC_LIBRARY})
endfunction()

add_demo(demo1_methods  src/demo1_methods.cpp)   # SC_METHOD, sc_signal, sensitivity
add_demo(demo2_threads  src/demo2_threads.cpp)   # SC_THREAD, wait(), sc_event
add_demo(demo3_fifos    src/demo3_fifos.cpp)     # sc_fifo, producer/consumer
add_demo(demo4_hierarchy src/demo4_hierarchy.cpp) # Hierarchical modules, sc_port, sc_export
add_demo(demo5_cthread  src/demo5_cthread.cpp)   # SC_CTHREAD, reset, VCD tracing
add_demo(demo6_datatypes src/demo6_datatypes.cpp) # sc_uint, sc_int, sc_bv, sc_lv
add_demo(demo7_mutex     src/demo7_mutex.cpp)     # sc_mutex, sc_semaphore
add_demo(demo8_multiclk  src/demo8_multiclk.cpp)  # Multiple clock domains, CDC
add_demo(demo9_spawn     src/demo9_spawn.cpp)     # sc_spawn, dynamic processes
add_demo(demo10_reporting src/demo10_reporting.cpp) # SC_REPORT, sc_time, phases

# TLM-2.0 demos
add_demo(demo11_tlm_lt    src/demo11_tlm_lt.cpp)          # Blocking transport (LT)
add_demo(demo12_tlm_at    src/demo12_tlm_at.cpp)          # Non-blocking transport (AT)
add_demo(demo13_tlm_dmi   src/demo13_tlm_dmi.cpp)         # Direct Memory Interface
add_demo(demo14_tlm_interconnect src/demo14_tlm_interconnect.cpp) # Bus, address decoding
add_demo(demo15_tlm_extensions   src/demo15_tlm_extensions.cpp)   # Extensions, temporal decoupling
add_demo(demo16_tlm_channels    src/demo16_tlm_channels.cpp)     # tlm_fifo, analysis_port, req_rsp
