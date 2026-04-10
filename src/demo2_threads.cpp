// Demo 2: SC_THREAD, wait(), sc_event
// =====================================
// Key concepts:
//   - SC_THREAD: a process that runs as a coroutine (can call wait())
//   - sc_event: a lightweight notification mechanism (like a condition variable)
//   - wait(event): suspend thread until event fires
//   - wait(time): suspend thread for a duration
//   - event.notify(): immediate or timed notification
//   - SC_THREAD runs once from start to finish (use while(true) to loop)
//
// This demo: a producer notifies a consumer via events with timed delays.

#include <systemc.h>

SC_MODULE(Producer) {
    sc_event& data_ready;   // reference to a shared event
    int value;

    void run() {
        int i = 0;
        while (true) {
            wait(15, SC_NS);               // model some processing time
            value = i * 10;
            std::cout << sc_time_stamp() << " Producer: value=" << value
                      << " (notifying consumer)" << std::endl;
            data_ready.notify();            // immediate notification
            i++;

            if (i > 5) {
                std::cout << sc_time_stamp() << " Producer: done" << std::endl;
                return;                     // thread exits
            }
        }
    }

    Producer(sc_module_name name, sc_event& evt)
        : sc_module(name), data_ready(evt), value(0) {
        SC_THREAD(run);
    }
};

SC_MODULE(Consumer) {
    sc_event& data_ready;
    int& shared_value;      // reference to producer's value

    void run() {
        while (true) {
            wait(data_ready);              // block until event fires
            std::cout << sc_time_stamp() << " Consumer: got value="
                      << shared_value << std::endl;
        }
    }

    Consumer(sc_module_name name, sc_event& evt, int& val)
        : sc_module(name), data_ready(evt), shared_value(val) {
        SC_THREAD(run);
    }
};

// Demonstrate timed notifications
SC_MODULE(TimedNotifier) {
    sc_event ping;

    void sender() {
        wait(5, SC_NS);
        std::cout << "\n" << sc_time_stamp()
                  << " TimedNotifier: notify(SC_ZERO_TIME) — "
                  << "fires in next delta cycle" << std::endl;
        ping.notify(SC_ZERO_TIME);          // next delta cycle

        wait(20, SC_NS);
        std::cout << sc_time_stamp()
                  << " TimedNotifier: notify(10, SC_NS) — "
                  << "fires 10ns later" << std::endl;
        ping.notify(10, SC_NS);             // 10ns from now
    }

    void receiver() {
        while (true) {
            wait(ping);
            std::cout << sc_time_stamp()
                      << " TimedNotifier: receiver woke up!" << std::endl;
        }
    }

    SC_CTOR(TimedNotifier) {
        SC_THREAD(sender);
        SC_THREAD(receiver);
        // A module can have multiple SC_THREADs — they run concurrently
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 2: SC_THREAD, wait(), sc_event ===\n" << std::endl;

    std::cout << "--- Part A: Producer/Consumer with events ---" << std::endl;
    sc_event data_evt;
    Producer prod("prod", data_evt);
    Consumer cons("cons", data_evt, prod.value);
    TimedNotifier tn("tn");

    sc_start(100, SC_NS);

    std::cout << "\n--- Part B: Timed notifications ---" << std::endl;
    sc_start(50, SC_NS);

    return 0;
}
