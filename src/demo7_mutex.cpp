// Demo 7: sc_mutex and sc_semaphore — shared resource access
// =============================================================
// Key concepts:
//   - sc_mutex: mutual exclusion — only one process can hold it
//     - lock(): blocking acquire
//     - trylock(): non-blocking, returns -1 if already locked
//     - unlock(): release
//   - sc_semaphore: counting semaphore — N processes can hold it
//     - wait(): decrement (block if 0)
//     - trywait(): non-blocking, returns -1 if 0
//     - post(): increment (release)
//   - Both are channels — no need for sc_signal wiring
//
// This demo: 3 CPUs competing for a shared bus (mutex), and
// 2 DMA engines sharing a pool of 2 channels (semaphore).

#include <systemc.h>

// ---- Shared bus with mutex ----
SC_MODULE(BusMaster) {
    int id;
    sc_mutex& bus;

    void run() {
        while (true) {
            wait(rand() % 20 + 5, SC_NS);  // random delay before request

            std::cout << sc_time_stamp() << " CPU" << id
                      << ": requesting bus..." << std::endl;

            bus.lock();     // BLOCKS until bus is free

            std::cout << sc_time_stamp() << " CPU" << id
                      << ": *** GOT bus, transferring ***" << std::endl;

            wait(15, SC_NS);  // hold bus during transfer

            std::cout << sc_time_stamp() << " CPU" << id
                      << ": releasing bus" << std::endl;

            bus.unlock();
        }
    }

    SC_HAS_PROCESS(BusMaster);
    BusMaster(sc_module_name name, int cpu_id, sc_mutex& m)
        : sc_module(name), id(cpu_id), bus(m) {
        SC_THREAD(run);
    }
};

// Demonstrate trylock (non-blocking)
SC_MODULE(PollingMaster) {
    sc_mutex& bus;

    void run() {
        for (int attempt = 0; attempt < 6; attempt++) {
            wait(10, SC_NS);

            if (bus.trylock() == 0) {   // 0 = success, -1 = locked
                std::cout << sc_time_stamp()
                          << " Poller: trylock SUCCESS, using bus" << std::endl;
                wait(8, SC_NS);
                bus.unlock();
            } else {
                std::cout << sc_time_stamp()
                          << " Poller: trylock FAILED, bus busy" << std::endl;
            }
        }
    }

    SC_HAS_PROCESS(PollingMaster);
    PollingMaster(sc_module_name name, sc_mutex& m)
        : sc_module(name), bus(m) {
        SC_THREAD(run);
    }
};

// ---- DMA with semaphore (pool of N channels) ----
SC_MODULE(DmaEngine) {
    int id;
    sc_semaphore& channels;  // counting semaphore

    void run() {
        for (int job = 0; job < 3; job++) {
            wait(rand() % 10 + 5, SC_NS);

            std::cout << sc_time_stamp() << " DMA" << id
                      << ": need a channel (avail="
                      << channels.get_value() << ")..." << std::endl;

            channels.wait();   // decrement; blocks if count==0

            std::cout << sc_time_stamp() << " DMA" << id
                      << ": got channel, transferring job " << job
                      << std::endl;

            wait(25, SC_NS);   // DMA transfer time

            std::cout << sc_time_stamp() << " DMA" << id
                      << ": done, releasing channel" << std::endl;

            channels.post();   // increment (release)
        }
    }

    SC_HAS_PROCESS(DmaEngine);
    DmaEngine(sc_module_name name, int dma_id, sc_semaphore& sem)
        : sc_module(name), id(dma_id), channels(sem) {
        SC_THREAD(run);
    }
};

int sc_main(int argc, char* argv[]) {
    srand(7);

    std::cout << "=== Demo 7: sc_mutex and sc_semaphore ===" << std::endl;

    // --- Part A: Mutex (exclusive access) ---
    std::cout << "\n--- Part A: 3 CPUs + 1 poller competing for 1 bus ---"
              << std::endl;

    sc_mutex bus("bus");

    BusMaster    cpu0("cpu0", 0, bus);
    BusMaster    cpu1("cpu1", 1, bus);
    BusMaster    cpu2("cpu2", 2, bus);
    PollingMaster poller("poller", bus);

    sc_start(120, SC_NS);

    // --- Part B: Semaphore (counted access) ---
    std::cout << "\n--- Part B: 4 DMA engines sharing 2 channels ---"
              << std::endl;

    sc_semaphore dma_channels("dma_channels", 2);  // pool of 2

    DmaEngine dma0("dma0", 0, dma_channels);
    DmaEngine dma1("dma1", 1, dma_channels);
    DmaEngine dma2("dma2", 2, dma_channels);
    DmaEngine dma3("dma3", 3, dma_channels);

    sc_start(300, SC_NS);

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "sc_mutex:     1 holder at a time (like std::mutex)" << std::endl;
    std::cout << "sc_semaphore: N holders at a time (counting semaphore)" << std::endl;
    std::cout << "Both block SC_THREADs — don't use in SC_METHOD!" << std::endl;

    return 0;
}
