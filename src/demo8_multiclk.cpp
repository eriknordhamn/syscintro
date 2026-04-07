// Demo 8: Multiple clock domains and clock-domain crossing
// ==========================================================
// Key concepts:
//   - sc_clock: built-in clock generator with configurable period/duty/phase
//   - Multiple sc_clocks can coexist at different frequencies
//   - SC_METHOD/SC_CTHREAD each sensitive to their own clock
//   - Clock domain crossing (CDC): passing data between domains
//     needs synchronization to avoid metastability
//   - A 2-FF synchronizer is the classic CDC solution
//
// This demo: a fast producer (100MHz), a slow consumer (25MHz),
// and a 2-FF synchronizer bridging the domains.

#include <systemc.h>

// ---- Fast domain: 100MHz counter ----
SC_MODULE(FastCounter) {
    sc_in<bool>         clk;
    sc_in<bool>         rst;
    sc_out<sc_uint<4>>  count;

    sc_uint<4> n;

    void tick() {
        if (rst.read()) {
            n = 0;
        } else {
            n++;
        }
        count.write(n);
        std::cout << sc_time_stamp() << " [FAST  100MHz] count=" << n << std::endl;
    }

    SC_CTOR(FastCounter) {
        SC_METHOD(tick);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---- Slow domain: 25MHz sampler ----
SC_MODULE(SlowSampler) {
    sc_in<bool>        clk;
    sc_in<sc_uint<4>>  data_in;

    void sample() {
        std::cout << sc_time_stamp() << " [SLOW   25MHz] sampled="
                  << data_in.read() << std::endl;
    }

    SC_CTOR(SlowSampler) {
        SC_METHOD(sample);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---- 2-FF Synchronizer (classic CDC technique) ----
// Passes a single-bit signal from one clock domain to another
// through two flip-flops to reduce metastability risk.
SC_MODULE(Synchronizer) {
    sc_in<bool>  clk;       // destination domain clock
    sc_in<bool>  rst;
    sc_in<bool>  async_in;  // from source domain (asynchronous!)
    sc_out<bool> sync_out;  // synchronized to destination domain

    bool ff1, ff2;

    void sync() {
        if (rst.read()) {
            ff1 = false;
            ff2 = false;
        } else {
            ff2 = ff1;              // second FF captures ff1
            ff1 = async_in.read();  // first FF captures async input
        }
        sync_out.write(ff2);

        std::cout << sc_time_stamp() << " [SYNC] async_in="
                  << async_in.read() << " ff1=" << ff1
                  << " ff2=" << ff2 << " sync_out=" << ff2 << std::endl;
    }

    SC_CTOR(Synchronizer) : ff1(false), ff2(false) {
        SC_METHOD(sync);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---- Pulse generator in fast domain ----
SC_MODULE(PulseGen) {
    sc_in<bool>  clk;
    sc_in<bool>  rst;
    sc_out<bool> pulse;

    int cycle;

    void tick() {
        if (rst.read()) {
            pulse.write(false);
            cycle = 0;
        } else {
            // Generate a pulse every 8 fast cycles
            pulse.write(cycle % 8 == 0);
            cycle++;
        }
    }

    SC_CTOR(PulseGen) : cycle(0) {
        SC_METHOD(tick);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---- Pulse detector in slow domain ----
SC_MODULE(PulseDetect) {
    sc_in<bool> clk;
    sc_in<bool> synced_pulse;

    bool prev;

    void detect() {
        bool cur = synced_pulse.read();
        if (cur && !prev) {
            std::cout << sc_time_stamp()
                      << " [DETECT] *** Rising edge detected in slow domain! ***"
                      << std::endl;
        }
        prev = cur;
    }

    SC_CTOR(PulseDetect) : prev(false) {
        SC_METHOD(detect);
        sensitive << clk.pos();
        dont_initialize();
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 8: Multiple clock domains ===" << std::endl;

    // Two clocks at different frequencies
    sc_clock fast_clk("fast_clk", 10, SC_NS);   // 100 MHz (10ns period)
    sc_clock slow_clk("slow_clk", 40, SC_NS);   // 25 MHz  (40ns period)

    sc_signal<bool>       rst;
    sc_signal<sc_uint<4>> count;
    sc_signal<bool>       pulse_raw;
    sc_signal<bool>       pulse_synced;

    // --- Part A: unsynchronized sampling (shows the problem) ---
    std::cout << "\n--- Part A: Fast counter, slow sampler (no sync) ---"
              << std::endl;
    std::cout << "The slow sampler will miss intermediate values!\n" << std::endl;

    FastCounter fc("fc");
    fc.clk(fast_clk); fc.rst(rst); fc.count(count);

    SlowSampler ss("ss");
    ss.clk(slow_clk); ss.data_in(count);

    // Reset
    rst.write(true);
    sc_start(15, SC_NS);
    rst.write(false);
    sc_start(160, SC_NS);

    // --- Part B: 2-FF synchronizer for single-bit CDC ---
    std::cout << "\n--- Part B: 2-FF synchronizer (proper CDC) ---" << std::endl;
    std::cout << "Pulse generated in fast domain, synchronized to slow domain\n"
              << std::endl;

    PulseGen pg("pg");
    pg.clk(fast_clk); pg.rst(rst); pg.pulse(pulse_raw);

    Synchronizer sync("sync");
    sync.clk(slow_clk); sync.rst(rst);
    sync.async_in(pulse_raw); sync.sync_out(pulse_synced);

    PulseDetect pd("pd");
    pd.clk(slow_clk); pd.synced_pulse(pulse_synced);

    rst.write(true);
    sc_start(15, SC_NS);
    rst.write(false);
    sc_start(320, SC_NS);

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Multiple sc_clock instances = multiple clock domains" << std::endl;
    std::cout << "Crossing domains without sync = metastability risk" << std::endl;
    std::cout << "2-FF synchronizer = classic single-bit CDC solution" << std::endl;

    return 0;
}
