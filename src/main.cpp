#include <systemc.h>
#include "mesh.h"
#include "txlog.h"

static const int MESH_SIZE = 4;
static const int SIM_TIME_NS = 2000;

int sc_main(int argc, char* argv[]) {
    srand(42);

    TxLog::instance().open("transactions.csv");

    Mesh mesh("mesh");
    mesh.build(MESH_SIZE);

    std::cout << "=== " << MESH_SIZE << "x" << MESH_SIZE
              << " Mesh Network Simulation ===" << std::endl;
    std::cout << "Running for " << SIM_TIME_NS << " ns..." << std::endl;

    sc_start(SIM_TIME_NS, SC_NS);

    mesh.print_stats();
    TxLog::instance().close();

    std::cout << "\nSimulation complete at " << sc_time_stamp() << std::endl;

    return 0;
}
