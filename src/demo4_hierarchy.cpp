// Demo 4: Hierarchical modules, sc_port, sc_export, custom interfaces
// =====================================================================
// Key concepts:
//   - Modules can contain sub-modules (hierarchy)
//   - sc_port<IF>: a port requiring an interface (connects outward)
//   - sc_export<IF>: exposes an interface from inside a module
//   - sc_interface: base class for custom channel interfaces
//   - Port binding: parent ports forward to child ports
//
// This demo: a System-on-Chip with CPU and Memory sub-modules,
// connected through a custom bus interface.

#include <systemc.h>

// ---- Custom interface: a simple bus ----
// An interface is a pure virtual class — it defines WHAT you can do,
// not HOW (that's the channel's job).
class bus_if : virtual public sc_interface {
public:
    virtual void bus_write(unsigned addr, int data) = 0;
    virtual int  bus_read(unsigned addr) = 0;
};

// ---- Channel implementing the bus interface ----
// A channel is the concrete implementation of an interface.
class SimpleBus : public sc_channel, public bus_if {
    int memory[64];

public:
    SimpleBus(sc_module_name name) : sc_channel(name) {
        memset(memory, 0, sizeof(memory));
    }

    void bus_write(unsigned addr, int data) override {
        if (addr < 64) {
            memory[addr] = data;
            std::cout << sc_time_stamp() << " Bus: WRITE addr=" << addr
                      << " data=" << data << std::endl;
            wait(5, SC_NS);    // model bus latency
        }
    }

    int bus_read(unsigned addr) override {
        if (addr < 64) {
            std::cout << sc_time_stamp() << " Bus: READ  addr=" << addr
                      << " data=" << memory[addr] << std::endl;
            wait(5, SC_NS);    // model bus latency
            return memory[addr];
        }
        return -1;
    }
};

// ---- CPU module: uses sc_port to access the bus ----
SC_MODULE(Cpu) {
    // sc_port<IF> — this module REQUIRES a bus_if to be connected
    sc_port<bus_if> bus_port;

    void run() {
        std::cout << sc_time_stamp() << " CPU: writing values..." << std::endl;

        // Access the interface through the port using ->
        bus_port->bus_write(0, 42);
        bus_port->bus_write(1, 99);
        bus_port->bus_write(2, 7);

        std::cout << sc_time_stamp() << " CPU: reading back..." << std::endl;

        int a = bus_port->bus_read(0);
        int b = bus_port->bus_read(1);
        int c = bus_port->bus_read(2);

        std::cout << sc_time_stamp() << " CPU: results: "
                  << a << ", " << b << ", " << c << std::endl;
    }

    SC_CTOR(Cpu) { 
        SC_THREAD(run); 
    }
};

// ---- DMA module: also uses the bus ----
SC_MODULE(Dma) {
    sc_port<bus_if> bus_port;

    void run() {
        wait(50, SC_NS);   // start after CPU
        std::cout << "\n" << sc_time_stamp()
                  << " DMA: copying addr 0-2 to addr 10-12" << std::endl;

        for (int i = 0; i < 3; i++) {
            int val = bus_port->bus_read(i);
            bus_port->bus_write(10 + i, val);
        }
        std::cout << sc_time_stamp() << " DMA: copy complete" << std::endl;
    }

    SC_CTOR(Dma) { 
        SC_THREAD(run); 
    }
};

// ---- Top-level SoC: hierarchical module ----
// This module contains sub-modules and wires them together.
SC_MODULE(SoC) {
    // Sub-modules (children in the hierarchy)
    Cpu       cpu;
    Dma       dma;
    SimpleBus bus;

    SC_CTOR(SoC)
        : cpu("cpu"),       // construct sub-modules with names
          dma("dma"),
          bus("bus")
    {
        // Bind ports to the channel
        cpu.bus_port(bus);  // cpu.bus_port connects to 'bus' channel
        dma.bus_port(bus);  // dma.bus_port connects to same 'bus' channel
        // Multiple ports can bind to the same channel
    }
};

// Demonstrate sc_export
SC_MODULE(Peripheral) {
    // sc_export exposes an interface that this module PROVIDES
    // (opposite of sc_port which REQUIRES an interface)
    sc_export<bus_if> target;

    SimpleBus internal_bus;

    SC_CTOR(Peripheral)
        : internal_bus("internal_bus")
    {
        // Export the internal channel's interface
        target(internal_bus);
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 4: Hierarchy, sc_port, sc_export ===" << std::endl;
    std::cout << "SoC with CPU + DMA sharing a bus\n" << std::endl;

    SoC soc("soc");

    // You can browse the hierarchy at runtime:
    std::cout << "Module hierarchy:" << std::endl;
    std::cout << "  " << soc.name() << std::endl;
    std::cout << "    " << soc.cpu.name() << std::endl;
    std::cout << "    " << soc.dma.name() << std::endl;
    std::cout << "    " << soc.bus.name() << std::endl;
    std::cout << std::endl;

    sc_start();
    return 0;
}
