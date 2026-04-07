// Demo 9: sc_spawn — dynamic process creation
// ===============================================
// Key concepts:
//   - sc_spawn(): create a new SC_THREAD at runtime (not just in constructors)
//   - sc_process_handle: a handle to control/query a spawned process
//   - sc_spawn_options: configure sensitivity, dont_initialize, etc.
//   - Useful for: fork/join parallelism, dynamic hardware, test generators
//   - SC_FORK/SC_JOIN macros: structured parallel execution
//
// This demo: a task manager that spawns worker threads dynamically,
// and a fork/join example for parallel DMA transfers.

#include <systemc.h>

// ---- Dynamic worker spawning ----
SC_MODULE(TaskManager) {
    sc_in<bool> clk;

    int next_id;

    // A worker function — each spawned process runs this independently
    void worker(int id, int work_time_ns) {
        std::cout << sc_time_stamp() << " Worker " << id
                  << ": started (will take " << work_time_ns << "ns)"
                  << std::endl;

        wait(work_time_ns, SC_NS);   // simulate work

        std::cout << sc_time_stamp() << " Worker " << id
                  << ": DONE" << std::endl;
    }

    void run() {
        wait(5, SC_NS);

        // Spawn 3 workers with different durations
        // sc_spawn takes a function object (use sc_bind or lambda)
        std::cout << sc_time_stamp() << " Manager: spawning 3 workers\n"
                  << std::endl;

        sc_process_handle h1 = sc_spawn(
            sc_bind(&TaskManager::worker, this, 0, 30));

        sc_process_handle h2 = sc_spawn(
            sc_bind(&TaskManager::worker, this, 1, 50));

        sc_process_handle h3 = sc_spawn(
            sc_bind(&TaskManager::worker, this, 2, 20));

        // Query process state
        std::cout << sc_time_stamp() << " Manager: h1 valid="
                  << h1.valid() << std::endl;

        // Wait for all to finish
        wait(60, SC_NS);
        std::cout << sc_time_stamp() << " Manager: all workers should be done"
                  << std::endl;
    }

    SC_CTOR(TaskManager) : next_id(0) {
        SC_THREAD(run);
    }
};

// ---- Fork/Join: parallel DMA transfers ----
SC_MODULE(DmaController) {

    void dma_transfer(int channel, unsigned src, unsigned dst, int bytes) {
        int latency = bytes / 4;  // 4 bytes per ns
        std::cout << sc_time_stamp() << " DMA ch" << channel
                  << ": transferring " << bytes << "B from 0x" << std::hex
                  << src << " to 0x" << dst << std::dec
                  << " (will take " << latency << "ns)" << std::endl;

        wait(latency, SC_NS);

        std::cout << sc_time_stamp() << " DMA ch" << channel
                  << ": transfer complete" << std::endl;
    }

    void run() {
        wait(100, SC_NS);  // start after TaskManager demo

        std::cout << "\n--- Fork/Join: parallel DMA ---" << std::endl;
        std::cout << sc_time_stamp() << " DMA: launching 3 parallel transfers"
                  << std::endl;

        // SC_FORK/SC_JOIN runs processes in parallel and waits for ALL
        sc_process_handle handles[3];

        handles[0] = sc_spawn(
            sc_bind(&DmaController::dma_transfer, this, 0, 0x1000, 0x2000, 64));
        handles[1] = sc_spawn(
            sc_bind(&DmaController::dma_transfer, this, 1, 0x3000, 0x4000, 128));
        handles[2] = sc_spawn(
            sc_bind(&DmaController::dma_transfer, this, 2, 0x5000, 0x6000, 32));

        // Wait for all three to complete
        for (int i = 0; i < 3; i++) {
            if (handles[i].valid()) {
                wait(handles[i].terminated_event());
            }
        }

        std::cout << sc_time_stamp()
                  << " DMA: all transfers complete (joined)" << std::endl;

        // --- Demonstrate sc_spawn_options ---
        std::cout << "\n--- Spawn with options ---" << std::endl;

        sc_spawn_options opts;
        opts.dont_initialize();         // don't run immediately
        opts.set_sensitivity(&clk);     // make it sensitive to clock

        // This spawned process behaves like an SC_METHOD
        int trigger_count = 0;
        sc_spawn([&trigger_count]() {
            std::cout << sc_time_stamp()
                      << " Spawned lambda running!" << std::endl;
        }, "lambda_process");

        wait(10, SC_NS);
    }

    sc_in<bool> clk;

    SC_CTOR(DmaController) {
        SC_THREAD(run);
    }
};

// ---- Demonstrate spawning from sc_main ----
void standalone_thread() {
    wait(180, SC_NS);
    std::cout << "\n" << sc_time_stamp()
              << " Standalone: spawned from sc_main!" << std::endl;
    std::cout << sc_time_stamp()
              << " Standalone: sc_spawn works outside modules too"
              << std::endl;
}

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 9: sc_spawn — dynamic process creation ===" << std::endl;
    std::cout << std::endl;

    sc_clock clk("clk", 10, SC_NS);

    TaskManager tm("tm");
    tm.clk(clk);

    DmaController dma("dma");
    dma.clk(clk);

    // Spawn a process from sc_main (before sc_start)
    sc_spawn(&standalone_thread, "standalone");

    sc_start(200, SC_NS);

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "sc_spawn(): create threads at runtime, not just in constructors"
              << std::endl;
    std::cout << "sc_process_handle: check valid(), wait on terminated_event()"
              << std::endl;
    std::cout << "sc_spawn_options: set sensitivity, dont_initialize" << std::endl;
    std::cout << "sc_bind / lambdas: pass member functions or closures" << std::endl;

    return 0;
}
