// Demo 1: SC_METHOD, sc_signal, sensitivity lists
// =================================================
// Key concepts:
//   - SC_METHOD: a process that runs like a callback (cannot block/wait)
//   - sc_signal<T>: a channel that models wire-like communication (1-delta delay)
//   - sensitive: tells SystemC when to re-trigger the method
//   - sc_in<T> / sc_out<T>: typed ports bound to signals
//
// This demo: an inverter and an AND gate wired together combinationally.

#include <systemc.h>

// ---- Inverter: out = ~in ----
SC_MODULE(Inverter) {
    sc_in<bool>  in;
    sc_out<bool> out;

    void compute() {
        out.write(!in.read());
        std::cout << sc_time_stamp() << " Inverter: in=" << in.read()
                  << " out=" << !in.read() << std::endl;
    }

    SC_CTOR(Inverter) {
        SC_METHOD(compute);     // registered as a method process
        sensitive << in;        // re-run whenever 'in' changes
        // NOTE: SC_METHOD cannot call wait(). It runs and returns.
    }
};

// ---- AND gate: out = a & b ----
SC_MODULE(AndGate) {
    sc_in<bool>  a;
    sc_in<bool>  b;
    sc_out<bool> out;

    void compute() {
        bool result = a.read() && b.read();
        out.write(result);
        std::cout << sc_time_stamp() << " AndGate: a=" << a.read()
                  << " b=" << b.read() << " out=" << result << std::endl;
    }

    SC_CTOR(AndGate) {
        SC_METHOD(compute);
        sensitive << a << b;    // re-run when EITHER input changes
    }
};

// ---- Testbench: drives stimulus ----
SC_MODULE(Testbench) {
    sc_out<bool> sig_a;
    sc_out<bool> sig_b;

    void drive() {
        // SC_THREAD can call wait() — it suspends and resumes
        sig_a.write(false); sig_b.write(false);
        wait(10, SC_NS);

        sig_a.write(true); sig_b.write(false);
        wait(10, SC_NS);

        sig_a.write(true); sig_b.write(true);
        wait(10, SC_NS);

        sig_a.write(false); sig_b.write(true);
        wait(10, SC_NS);

        sc_stop();
    }

    SC_CTOR(Testbench) {
        SC_THREAD(drive);       // thread process — runs once, can wait()
    }
};

int sc_main(int argc, char* argv[]) {
    // Signals are the "wires" connecting ports
    sc_signal<bool> a, b, not_a, result;

    Inverter inv("inv");
    inv.in(a);
    inv.out(not_a);

    AndGate gate("gate");
    gate.a(not_a);          // AND gets the inverted 'a'
    gate.b(b);
    gate.out(result);

    Testbench tb("tb");
    tb.sig_a(a);
    tb.sig_b(b);

    std::cout << "=== Demo 1: SC_METHOD, sc_signal, sensitivity ===" << std::endl;
    std::cout << "Circuit: result = (~a) & b\n" << std::endl;

    sc_start();
    return 0;
}
