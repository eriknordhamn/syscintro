// Demo 14: TLM-2.0 Interconnect with Address Decoding
// =====================================================
// Key concepts:
//   - A bus/interconnect routes transactions based on address
//   - Multiple initiators, multiple targets
//   - simple_target_socket_tagged: tags identify which initiator called
//   - simple_initiator_socket_tagged: tags identify which target port
//   - Address map: each target owns an address range
//   - The interconnect translates addresses (subtracts base) before forwarding
//   - This pattern builds real SoC platforms
//
// This demo: 2 CPUs, 1 bus, 3 targets (RAM, ROM, UART peripheral).

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

using namespace tlm;
using namespace tlm_utils;

// ---- Generic memory target ----
SC_MODULE(MemTarget) {
    simple_target_socket<MemTarget> socket;
    unsigned char* mem;
    unsigned int   size;
    std::string    label;
    bool           readonly;

    MemTarget(sc_module_name name, unsigned int sz,
              bool ro = false, unsigned char fill = 0)
        : sc_module(name), socket("socket"), size(sz), readonly(ro) {
        label = std::string(name);
        mem = new unsigned char[sz];
        memset(mem, fill, sz);
        socket.register_b_transport(this, &MemTarget::b_transport);
    }

    ~MemTarget() { delete[] mem; }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        tlm_command    cmd  = trans.get_command();
        uint64_t       addr = trans.get_address();  // already translated!
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int   len  = trans.get_data_length();

        if (addr + len > size) {
            std::cout << sc_time_stamp() << " " << label
                      << ": ADDRESS ERROR @ 0x" << std::hex << addr
                      << std::dec << std::endl;
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (cmd == TLM_WRITE_COMMAND) {
            if (readonly) {
                std::cout << sc_time_stamp() << " " << label
                          << ": WRITE to read-only memory!" << std::endl;
                trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
                return;
            }
            memcpy(&mem[addr], ptr, len);
            std::cout << sc_time_stamp() << " " << label
                      << ": WRITE " << len << "B @ offset 0x"
                      << std::hex << addr << std::dec << std::endl;
        } else {
            memcpy(ptr, &mem[addr], len);
            std::cout << sc_time_stamp() << " " << label
                      << ": READ  " << len << "B @ offset 0x"
                      << std::hex << addr << std::dec << std::endl;
        }

        delay += sc_time(10, SC_NS);
        trans.set_response_status(TLM_OK_RESPONSE);
    }
};

// ---- UART peripheral ----
SC_MODULE(Uart) {
    simple_target_socket<Uart> socket;

    // Register offsets
    static const uint64_t REG_DATA   = 0x00;  // write: TX, read: RX
    static const uint64_t REG_STATUS = 0x04;  // bit 0: TX ready, bit 1: RX avail

    SC_CTOR(Uart) : socket("socket") {
        socket.register_b_transport(this, &Uart::b_transport);
    }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        uint64_t       addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();

        if (trans.get_command() == TLM_WRITE_COMMAND && addr == REG_DATA) {
            char ch = *ptr;
            std::cout << sc_time_stamp() << " UART TX: '"
                      << (isprint(ch) ? ch : '?') << "' (0x"
                      << std::hex << (int)(unsigned char)ch << std::dec
                      << ")" << std::endl;
        } else if (trans.get_command() == TLM_READ_COMMAND && addr == REG_STATUS) {
            uint32_t status = 0x01;  // TX always ready
            memcpy(ptr, &status, 4);
            std::cout << sc_time_stamp()
                      << " UART: status read = 0x01" << std::endl;
        } else {
            std::cout << sc_time_stamp()
                      << " UART: access @ 0x" << std::hex << addr
                      << std::dec << std::endl;
        }

        delay += sc_time(5, SC_NS);
        trans.set_response_status(TLM_OK_RESPONSE);
    }
};

// ---- Bus / Interconnect ----
// Routes transactions to targets based on address map.
struct AddrRange {
    uint64_t base;
    uint64_t size;
    int      target_idx;
};

SC_MODULE(Bus) {
    // Initiator-side ports (targets for the CPUs)
    simple_target_socket<Bus>* tgt_socket[4];
    // Target-side ports (initiators toward the targets)
    simple_initiator_socket<Bus>* init_socket[4];

    int n_targets;
    AddrRange map[4];

    Bus(sc_module_name name) : sc_module(name), n_targets(0) {
        for (int i = 0; i < 4; i++) {
            char nm[32];
            snprintf(nm, sizeof(nm), "tgt_socket_%d", i);
            tgt_socket[i] = new simple_target_socket<Bus>(nm);
            tgt_socket[i]->register_b_transport(this, &Bus::b_transport);

            snprintf(nm, sizeof(nm), "init_socket_%d", i);
            init_socket[i] = new simple_initiator_socket<Bus>(nm);
        }
    }

    void add_target(int idx, uint64_t base, uint64_t size) {
        map[idx] = {base, size, idx};
        if (idx >= n_targets) n_targets = idx + 1;
    }

    int decode(uint64_t addr) {
        for (int i = 0; i < n_targets; i++) {
            if (addr >= map[i].base && addr < map[i].base + map[i].size)
                return i;
        }
        return -1;
    }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        uint64_t addr = trans.get_address();
        int target = decode(addr);

        if (target < 0) {
            std::cout << sc_time_stamp() << " Bus: DECODE ERROR @ 0x"
                      << std::hex << addr << std::dec << std::endl;
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        // Translate address: subtract base so target sees offset
        uint64_t original_addr = addr;
        trans.set_address(addr - map[target].base);

        std::cout << sc_time_stamp() << " Bus: routing 0x" << std::hex
                  << original_addr << " -> target " << std::dec << target
                  << " offset 0x" << std::hex
                  << (original_addr - map[target].base)
                  << std::dec << std::endl;

        // Add bus traversal latency
        delay += sc_time(2, SC_NS);

        // Forward to target
        (*init_socket[target])->b_transport(trans, delay);

        // Restore original address
        trans.set_address(original_addr);
    }

    ~Bus() {
        for (int i = 0; i < 4; i++) {
            delete tgt_socket[i];
            delete init_socket[i];
        }
    }
};

// ---- CPU ----
SC_MODULE(Cpu) {
    simple_initiator_socket<Cpu> socket;
    int id;

    void do_write(uint64_t addr, uint32_t data, sc_time& delay) {
        tlm_generic_payload trans;
        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        if (trans.is_response_error())
            std::cout << "  CPU" << id << " ERROR: "
                      << trans.get_response_string() << std::endl;
    }

    uint32_t do_read(uint64_t addr, sc_time& delay) {
        uint32_t data = 0;
        tlm_generic_payload trans;
        trans.set_command(TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return data;
    }

    void run() {
        sc_time delay = SC_ZERO_TIME;

        if (id == 0) {
            std::cout << "\n--- CPU0: write to RAM, read from ROM ---" << std::endl;
            do_write(0x0000, 0xAAAA, delay);   // RAM @ 0x0000
            wait(delay); delay = SC_ZERO_TIME;

            uint32_t rom_val = do_read(0x4000, delay);  // ROM @ 0x4000
            wait(delay); delay = SC_ZERO_TIME;
            std::cout << "  CPU0: ROM read = 0x" << std::hex << rom_val
                      << std::dec << std::endl;

            std::cout << "\n--- CPU0: write to UART ---" << std::endl;
            do_write(0x8000, 'H', delay); wait(delay); delay = SC_ZERO_TIME;
            do_write(0x8000, 'i', delay); wait(delay); delay = SC_ZERO_TIME;
            do_write(0x8000, '!', delay); wait(delay); delay = SC_ZERO_TIME;

            std::cout << "\n--- CPU0: access unmapped address ---" << std::endl;
            do_write(0xF000, 0, delay);
            wait(delay); delay = SC_ZERO_TIME;

            std::cout << "\n--- CPU0: write to ROM (should fail) ---" << std::endl;
            do_write(0x4000, 0xBBBB, delay);
            wait(delay); delay = SC_ZERO_TIME;
        } else {
            wait(20, SC_NS);  // let CPU0 go first
            std::cout << "\n--- CPU1: read from RAM ---" << std::endl;
            uint32_t val = do_read(0x0000, delay);
            wait(delay); delay = SC_ZERO_TIME;
            std::cout << "  CPU1: RAM read = 0x" << std::hex << val
                      << std::dec << " (written by CPU0)" << std::endl;
        }
    }

    Cpu(sc_module_name name, int cpu_id)
        : sc_module(name), socket("socket"), id(cpu_id) {
        SC_THREAD(run);
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 14: TLM Interconnect with Address Decoding ===" << std::endl;
    std::cout << "Address map:" << std::endl;
    std::cout << "  0x0000-0x3FFF  RAM  (16KB, R/W)" << std::endl;
    std::cout << "  0x4000-0x7FFF  ROM  (16KB, R/O)" << std::endl;
    std::cout << "  0x8000-0x80FF  UART (256B, R/W)" << std::endl;

    // Create components
    Cpu  cpu0("cpu0", 0);
    Cpu  cpu1("cpu1", 1);
    Bus  bus("bus");

    MemTarget ram("RAM",  0x4000, false, 0x00);    // 16KB, R/W
    MemTarget rom("ROM",  0x4000, true,  0xFF);    // 16KB, read-only, filled 0xFF
    Uart      uart("UART");

    // Configure address map
    bus.add_target(0, 0x0000, 0x4000);  // RAM
    bus.add_target(1, 0x4000, 0x4000);  // ROM
    bus.add_target(2, 0x8000, 0x0100);  // UART

    // Bind: CPUs -> Bus -> Targets
    cpu0.socket.bind(*bus.tgt_socket[0]);
    cpu1.socket.bind(*bus.tgt_socket[1]);
    bus.init_socket[0]->bind(ram.socket);
    bus.init_socket[1]->bind(rom.socket);
    bus.init_socket[2]->bind(uart.socket);

    sc_start();
    return 0;
}
