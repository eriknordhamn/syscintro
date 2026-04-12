#include <systemc.h>
#include "mesh.h"
#include "txlog.h"

static const int MESH_SIZE = 4;
static const int SIM_TIME_NS = 2000;
static const int ROUTER_LATENCY_CYCLES = 1;
static const int CLOCK_PERIOD_NS       = 1;
static const int PIPE_STAGES           = 2;  // pipeline stages per inter-router link
static const int PIPE_EXTRA_BUFFER     = 1;  // extra FIFO slots per stage (beyond 1)

int sc_main(int argc, char* argv[]) {
    srand(42);

    TxLog::instance().open("transactions.csv");

    Mesh mesh("mesh");
    mesh.build(MESH_SIZE, ROUTER_LATENCY_CYCLES, sc_time(CLOCK_PERIOD_NS, SC_NS),
               PIPE_STAGES, PIPE_EXTRA_BUFFER);

    std::cout << "=== " << MESH_SIZE << "x" << MESH_SIZE
              << " Mesh Network Simulation ===" << std::endl;
    std::cout << "Running for " << SIM_TIME_NS << " ns..." << std::endl;

    sc_start(SIM_TIME_NS, SC_NS);

    mesh.print_stats();
    TxLog::instance().close();

    std::cout << "\nSimulation complete at " << sc_time_stamp() << std::endl;

    return 0;
}
