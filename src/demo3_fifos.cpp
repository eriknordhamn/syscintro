// Demo 3: sc_fifo — built-in FIFO channel
// ==========================================
// Key concepts:
//   - sc_fifo<T>: bounded FIFO channel (like SimPy Store)
//   - sc_fifo_in<T> / sc_fifo_out<T>: port types for FIFOs
//   - write()/read(): blocking (use in SC_THREAD)
//   - nb_write()/nb_read(): non-blocking, return bool
//   - num_available() / num_free(): query occupancy
//   - sc_fifo replaces manual std::queue when using SC_THREAD
//
// This demo: a data source fills a FIFO, a processor drains it at a
// different rate, showing backpressure naturally.

#include <systemc.h>

SC_MODULE(Source) {
    sc_fifo_out<int> out;   // port that connects to a sc_fifo

    void run() {
        for (int i = 0; i < 10; i++) {
            std::cout << sc_time_stamp() << " Source: writing " << i
                      << " (fifo free=" << out.num_free() << ")" << std::endl;

            out.write(i);   // BLOCKS if fifo is full — backpressure!

            std::cout << sc_time_stamp() << " Source: wrote " << i << std::endl;
            wait(5, SC_NS); // fast producer
        }
        std::cout << sc_time_stamp() << " Source: done" << std::endl;
    }

    SC_CTOR(Source) { SC_THREAD(run); }
};

SC_MODULE(Sink) {
    sc_fifo_in<int> in;     // port that connects to a sc_fifo

    void run() {
        while (true) {
            int val;

            // Non-blocking check first (just to demonstrate nb_read)
            if (in.num_available() == 0) {
                std::cout << sc_time_stamp()
                          << " Sink: fifo empty, waiting..." << std::endl;
            }

            val = in.read();    // BLOCKS if fifo is empty

            std::cout << sc_time_stamp() << " Sink: read " << val
                      << " (fifo avail=" << in.num_available() << ")"
                      << std::endl;
            wait(20, SC_NS);    // slow consumer — will cause backpressure
        }
    }

    SC_CTOR(Sink) { SC_THREAD(run); }
};

// Demonstrate nb_read / nb_write (non-blocking)
SC_MODULE(NonBlockingDemo) {
    sc_fifo_out<int> out;
    sc_fifo_in<int>  in;

    void writer() {
        wait(200, SC_NS);  // start after main demo
        std::cout << "\n--- Non-blocking demo ---" << std::endl;

        for (int i = 100; i < 106; i++) {
            // nb_write returns false if full, true if written
            if (out.nb_write(i)) {
                std::cout << sc_time_stamp()
                          << " NB-Writer: nb_write(" << i << ") OK" << std::endl;
            } else {
                std::cout << sc_time_stamp()
                          << " NB-Writer: nb_write(" << i << ") FULL!" << std::endl;
            }
            wait(2, SC_NS);
        }
    }

    void reader() {
        wait(210, SC_NS);
        int val;
        while (true) {
            // nb_read returns false if empty
            if (in.nb_read(val)) {
                std::cout << sc_time_stamp()
                          << " NB-Reader: nb_read() = " << val << std::endl;
            } else {
                std::cout << sc_time_stamp()
                          << " NB-Reader: nb_read() EMPTY" << std::endl;
                return;
            }
        }
    }

    SC_CTOR(NonBlockingDemo) {
        SC_THREAD(writer);
        SC_THREAD(reader);
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 3: sc_fifo — built-in FIFO channel ===" << std::endl;
    std::cout << "FIFO depth=4, fast producer (5ns), slow consumer (20ns)\n"
              << std::endl;

    // sc_fifo<T>(depth) — the channel itself
    sc_fifo<int> fifo(4);       // depth of 4

    Source src("src");
    src.out(fifo);              // bind port to fifo channel

    Sink snk("snk");
    snk.in(fifo);               // bind port to same fifo

    // Second fifo for non-blocking demo
    sc_fifo<int> nb_fifo(3);    // small fifo to show overflow

    NonBlockingDemo nbd("nbd");
    nbd.out(nb_fifo);
    nbd.in(nb_fifo);

    sc_start(250, SC_NS);
    return 0;
}
