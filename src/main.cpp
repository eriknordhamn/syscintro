#include <systemc.h>
#include "mesh.h"

static const int MESH_SIZE = 4;         // 4x4 mesh
static const int SIM_CYCLES = 200;      // simulation duration in clock cycles

int sc_main(int argc, char* argv[]) {
    srand(42);

    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst;

    Mesh mesh("mesh");
    mesh.clk(clk);
    mesh.rst(rst);
    mesh.build(MESH_SIZE);

    // Optional VCD trace
    sc_trace_file* tf = sc_create_vcd_trace_file("mesh_trace");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, rst, "rst");

    std::cout << "=== " << MESH_SIZE << "x" << MESH_SIZE
              << " Mesh Network Simulation ===" << std::endl;
    std::cout << "Running for " << SIM_CYCLES << " clock cycles ("
              << SIM_CYCLES * 10 << " ns)..." << std::endl;

    // Reset phase
    rst.write(true);
    sc_start(30, SC_NS);

    // Release reset and run
    rst.write(false);
    sc_start(SIM_CYCLES * 10, SC_NS);

    mesh.print_stats();

    sc_close_vcd_trace_file(tf);

    std::cout << "\nSimulation complete at " << sc_time_stamp() << std::endl;

    return 0;
}
