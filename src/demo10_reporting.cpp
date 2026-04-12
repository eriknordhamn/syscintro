// Demo 10: SC_REPORT, sc_time, simulation phases, callbacks
// ============================================================
// Key concepts:
//   - SC_REPORT_INFO/WARNING/ERROR/FATAL: structured logging with severity
//   - sc_report_handler: configure actions per severity (throw, log, stop)
//   - sc_time: time arithmetic, conversions, comparisons
//   - Simulation phases: elaboration -> simulation -> cleanup
//   - before_end_of_elaboration(), end_of_elaboration(),
//     start_of_simulation(), end_of_simulation(): phase callbacks
//   - sc_stop(): graceful simulation stop
//
// This demo: a watchdog timer that uses SC_REPORT, a module showing
// all simulation phases, and sc_time arithmetic.

#include <systemc.h>

// ---- Demonstrate simulation phase callbacks ----
SC_MODULE(LifecycleDemo) {
    sc_in<bool> clk;

    // These callbacks fire at specific simulation phases.
    // Override them to do setup, validation, or cleanup.

    void before_end_of_elaboration() override {
        std::cout << "[PHASE] before_end_of_elaboration: "
                  << name() << " — last chance to create ports/modules"
                  << std::endl;
    }

    void end_of_elaboration() override {
        std::cout << "[PHASE] end_of_elaboration: "
                  << name() << " — all ports bound, hierarchy fixed"
                  << std::endl;

        // Good place to validate configuration
        std::cout << "         Module hierarchy is now frozen." << std::endl;
    }

    void start_of_simulation() override {
        std::cout << "[PHASE] start_of_simulation: "
                  << name() << " — about to call sc_start()"
                  << std::endl;
    }

    void end_of_simulation() override {
        std::cout << "[PHASE] end_of_simulation: "
                  << name() << " — cleanup, print final stats"
                  << std::endl;
    }

    void tick() {
        // just a placeholder process
    }

    SC_CTOR(LifecycleDemo) {
        SC_METHOD(tick);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---- Watchdog using SC_REPORT ----
SC_MODULE(Watchdog) {
    sc_in<bool> clk;
    sc_in<bool> heartbeat;   // must pulse before timeout

    int cycles_since_beat;
    int timeout;

    void monitor() {
        if (heartbeat.read()) {
            cycles_since_beat = 0;
            // SC_REPORT_INFO: informational, severity = SC_INFO
            SC_REPORT_INFO("Watchdog", "Heartbeat received");
        } else {
            cycles_since_beat++;
        }

        if (cycles_since_beat == timeout / 2) {
            // SC_REPORT_WARNING: potential issue, severity = SC_WARNING
            SC_REPORT_WARNING("Watchdog",
                "No heartbeat for half the timeout period!");
        }

        if (cycles_since_beat >= timeout) {
            // SC_REPORT_ERROR: serious problem
            // Default action: throw sc_report (can be caught or changed)
            char msg[128];
            snprintf(msg, sizeof(msg),
                "TIMEOUT! No heartbeat for %d cycles", timeout);
            SC_REPORT_ERROR("Watchdog", msg);
        }
    }

    Watchdog(sc_module_name name, int tmo)
        : sc_module(name), cycles_since_beat(0), timeout(tmo) {
        SC_METHOD(monitor);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---- Heartbeat generator (stops beating to trigger watchdog) ----
SC_MODULE(Heart) {
    sc_in<bool>  clk;
    sc_out<bool> beat;

    int cycle;

    void pump() {
        cycle++;
        // Beat every 3 cycles, but stop after cycle 15
        if (cycle < 15) {
            beat.write(cycle % 3 == 0);
        } else {
            beat.write(false);  // stop beating — watchdog will fire!
        }
    }

    SC_CTOR(Heart) : cycle(0) {
        SC_METHOD(pump);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---- sc_time arithmetic ----
void demonstrate_time() {
    std::cout << "\n--- sc_time arithmetic ---" << std::endl;

    sc_time t1(10, SC_NS);
    sc_time t2(2.5, SC_NS);

    std::cout << "t1      = " << t1 << std::endl;
    std::cout << "t2      = " << t2 << std::endl;
    std::cout << "t1 + t2 = " << (t1 + t2) << std::endl;
    std::cout << "t1 - t2 = " << (t1 - t2) << std::endl;
    std::cout << "t1 * 3  = " << (t1 * 3) << std::endl;
    std::cout << "t1 / t2 = " << (t1 / t2) << " (ratio)" << std::endl;
    std::cout << "t1 > t2 = " << (t1 > t2) << std::endl;

    // Convert to different units
    std::cout << "t1 in ps = " << t1.to_double() / 1.0 << " "
              << t1.to_string() << std::endl;

    // Special values
    std::cout << "SC_ZERO_TIME = " << SC_ZERO_TIME << std::endl;

    // Time resolution
    std::cout << "Time resolution: " << sc_get_time_resolution() << std::endl;
}

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 10: SC_REPORT, sc_time, simulation phases ==="
              << std::endl;

    // --- Configure report handler ---
    // Change what happens on each severity level:
    //   SC_DO_NOTHING, SC_LOG, SC_DISPLAY, SC_THROW, SC_STOP, SC_ABORT
    sc_report_handler::set_actions(SC_INFO,    SC_DISPLAY);
    sc_report_handler::set_actions(SC_WARNING, SC_DISPLAY);
    sc_report_handler::set_actions(SC_ERROR,   SC_DISPLAY | SC_STOP);
    // Without SC_STOP on ERROR, it would throw an exception.
    // SC_FATAL always calls sc_stop() by default.

    // You can also suppress specific message types:
    // sc_report_handler::set_actions("Watchdog", SC_INFO, SC_DO_NOTHING);

    demonstrate_time();

    std::cout << "\n--- Creating modules (elaboration phase) ---" << std::endl;

    sc_clock        clk("clk", 10, SC_NS);
    sc_signal<bool> heartbeat;

    LifecycleDemo   lifecycle("lifecycle");
    lifecycle.clk(clk);

    Watchdog wd("wd", 8);   // timeout after 8 cycles without heartbeat
    wd.clk(clk);
    wd.heartbeat(heartbeat);

    Heart heart("heart");
    heart.clk(clk);
    heart.beat(heartbeat);

    std::cout << "\n--- Starting simulation ---" << std::endl;
    sc_start(300, SC_NS);

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "SC_REPORT_INFO/WARNING/ERROR/FATAL: structured severity levels"
              << std::endl;
    std::cout << "sc_report_handler: configure actions per severity" << std::endl;
    std::cout << "sc_time: full arithmetic, comparison, unit conversion" << std::endl;
    std::cout << "Phase callbacks: elaborate -> simulate -> cleanup" << std::endl;

    // Print report summary
    std::cout << "\nReport summary:" << std::endl;
    std::cout << "  Infos:    "
              << sc_report_handler::get_count(SC_INFO) << std::endl;
    std::cout << "  Warnings: "
              << sc_report_handler::get_count(SC_WARNING) << std::endl;
    std::cout << "  Errors:   "
              << sc_report_handler::get_count(SC_ERROR) << std::endl;

    return 0;
}
