// Demo 5: SC_CTHREAD, synchronous reset, sc_trace / VCD output
// ===============================================================
// Key concepts:
//   - SC_CTHREAD: clocked thread — like SC_THREAD but tied to a clock edge
//   - reset_signal_is(): declares a synchronous or async reset
//   - sc_trace / sc_trace_file: record signals to VCD for waveform viewing
//   - sc_clock: built-in clock generator
//   - Difference between SC_METHOD / SC_THREAD / SC_CTHREAD:
//       SC_METHOD  — combinational, cannot wait(), re-triggered by sensitivity
//       SC_THREAD  — free-running coroutine, manual wait()
//       SC_CTHREAD — coroutine synced to clock edge, wait() = next clock
//
// This demo: a shift register and a state machine, both using SC_CTHREAD,
// with VCD output you can view in GTKWave.

#include <systemc.h>

// ---- 4-bit shift register with synchronous reset ----
SC_MODULE(ShiftReg) {
    sc_in<bool>        clk;
    sc_in<bool>        rst;
    sc_in<bool>        din;
    sc_out<sc_uint<4>> dout;    // sc_uint<N>: N-bit unsigned integer

    sc_uint<4> reg;

    void shift() {
        // Code before the while(true) runs ONCE during reset
        reg = 0;
        dout.write(0);
        wait();                 // wait for reset to deassert

        // Main loop: runs on each clock edge
        while (true) {
            reg = (reg << 1) | din.read();
            dout.write(reg);
            std::cout << sc_time_stamp() << " ShiftReg: din=" << din.read()
                      << " reg=" << reg.to_string(SC_BIN) << std::endl;
            wait();             // wait for next clock edge
        }
    }

    SC_CTOR(ShiftReg) {
        SC_CTHREAD(shift, clk.pos());       // triggered on rising edge
        reset_signal_is(rst, true);         // rst=true means reset active
    }
};

// ---- Simple state machine (traffic light) ----
SC_MODULE(TrafficLight) {
    sc_in<bool>  clk;
    sc_in<bool>  rst;
    sc_out<int>  state_out;     // 0=RED, 1=GREEN, 2=YELLOW

    void fsm() {
        // Reset state
        state_out.write(0);
        wait();

        while (true) {
            // RED for 3 cycles
            state_out.write(0);
            std::cout << sc_time_stamp() << " Light: RED" << std::endl;
            wait(); wait(); wait();

            // GREEN for 4 cycles
            state_out.write(1);
            std::cout << sc_time_stamp() << " Light: GREEN" << std::endl;
            wait(); wait(); wait(); wait();

            // YELLOW for 2 cycles
            state_out.write(2);
            std::cout << sc_time_stamp() << " Light: YELLOW" << std::endl;
            wait(); wait();
        }
    }

    SC_CTOR(TrafficLight) {
        SC_CTHREAD(fsm, clk.pos());
        reset_signal_is(rst, true);
    }
};

// ---- Stimulus generator ----
SC_MODULE(Stimulus) {
    sc_in<bool>  clk;
    sc_out<bool> din;

    void run() {
        // Pattern: 1,0,1,1,0,1,0,0,1,1
        bool pattern[] = {1,0,1,1,0,1,0,0,1,1};
        for (int i = 0; i < 10; i++) {
            din.write(pattern[i]);
            wait();
        }
        din.write(false);
        wait();
    }

    SC_CTOR(Stimulus) {
        SC_CTHREAD(run, clk.pos());
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 5: SC_CTHREAD, reset, VCD tracing ===" << std::endl;

    // Clock: 10ns period (5ns high, 5ns low)
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool>        rst;
    sc_signal<bool>        din;
    sc_signal<sc_uint<4>>  shift_out;
    sc_signal<int>         light_state;

    ShiftReg   sr("sr");
    sr.clk(clk); sr.rst(rst); sr.din(din); sr.dout(shift_out);

    TrafficLight tl("tl");
    tl.clk(clk); tl.rst(rst); tl.state_out(light_state);

    Stimulus stim("stim");
    stim.clk(clk); stim.din(din);

    // ---- VCD trace setup ----
    // Creates a .vcd file viewable in GTKWave or similar
    sc_trace_file* vcd = sc_create_vcd_trace_file("demo5_waves");

    // Register signals to trace
    sc_trace(vcd, clk,         "clk");
    sc_trace(vcd, rst,         "rst");
    sc_trace(vcd, din,         "din");
    sc_trace(vcd, shift_out,   "shift_reg");
    sc_trace(vcd, light_state, "traffic_light");

    std::cout << "Writing VCD trace to demo5_waves.vcd" << std::endl;
    std::cout << "View with: gtkwave demo5_waves.vcd\n" << std::endl;

    // Reset phase
    rst.write(true);
    sc_start(25, SC_NS);       // hold reset for 2.5 clock cycles

    // Run
    rst.write(false);
    sc_start(200, SC_NS);

    sc_close_vcd_trace_file(vcd);

    std::cout << "\n=== Process type summary ===" << std::endl;
    std::cout << "SC_METHOD  — cannot wait(), re-runs on sensitivity list" << std::endl;
    std::cout << "SC_THREAD  — can wait(time) or wait(event), free-running" << std::endl;
    std::cout << "SC_CTHREAD — can wait(), always synced to clock edge" << std::endl;

    return 0;
}
