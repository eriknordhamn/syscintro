// Demo 13: TLM-2.0 Direct Memory Interface (DMI)
// =================================================
// Key concepts:
//   - DMI bypasses b_transport entirely for fast memory access
//   - Initiator calls get_direct_mem_ptr() to get a raw pointer
//   - Subsequent accesses use the pointer directly — no socket calls
//   - Huge simulation speedup for memory-heavy workloads
//   - Target can invalidate DMI via invalidate_direct_mem_ptr()
//     (e.g., when memory is remapped or device registers change)
//   - tlm_dmi: describes the valid DMI region (start, end, pointer, latency)
//
// This demo: CPU uses b_transport initially, then switches to DMI.
// Memory invalidates DMI mid-simulation to show the full flow.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

using namespace tlm;
using namespace tlm_utils;

// ---- Memory with DMI support ----
SC_MODULE(DmiMemory) {
    simple_target_socket<DmiMemory> socket;
    unsigned char mem[1024];
    bool dmi_enabled;

    SC_CTOR(DmiMemory) : socket("socket"), dmi_enabled(true) {
        socket.register_b_transport(this, &DmiMemory::b_transport);
        socket.register_get_direct_mem_ptr(this,
            &DmiMemory::get_direct_mem_ptr);
        memset(mem, 0, sizeof(mem));
    }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        uint64_t       addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int   len  = trans.get_data_length();

        if (addr + len > sizeof(mem)) {
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (trans.get_command() == TLM_WRITE_COMMAND)
            memcpy(&mem[addr], ptr, len);
        else
            memcpy(ptr, &mem[addr], len);

        delay += sc_time(10, SC_NS);  // normal access latency

        // Tell initiator that DMI is available
        trans.set_dmi_allowed(dmi_enabled);
        trans.set_response_status(TLM_OK_RESPONSE);

        std::cout << sc_time_stamp() << " Memory: b_transport "
                  << (trans.get_command() == TLM_WRITE_COMMAND ? "WRITE" : "READ")
                  << " @ 0x" << std::hex << addr << std::dec
                  << " (slow path)" << std::endl;
    }

    // DMI request: return a direct pointer to the memory array
    bool get_direct_mem_ptr(tlm_generic_payload& trans,
                            tlm_dmi& dmi_data) {
        if (!dmi_enabled) return false;

        std::cout << sc_time_stamp()
                  << " Memory: granting DMI access" << std::endl;

        // Set the DMI descriptor
        dmi_data.set_dmi_ptr(mem);                     // raw pointer
        dmi_data.set_start_address(0);                 // valid from addr 0
        dmi_data.set_end_address(sizeof(mem) - 1);     // to addr 1023
        dmi_data.allow_read_write();                   // both R and W OK

        // DMI latency (much faster than b_transport)
        dmi_data.set_read_latency(sc_time(1, SC_NS));
        dmi_data.set_write_latency(sc_time(1, SC_NS));

        return true;
    }

    // Call this to revoke DMI (e.g., memory remapped)
    void revoke_dmi() {
        dmi_enabled = false;
        std::cout << sc_time_stamp()
                  << " Memory: INVALIDATING DMI" << std::endl;

        // Notify initiator that DMI region is no longer valid
        socket->invalidate_direct_mem_ptr(0, sizeof(mem) - 1);
    }
};

// ---- CPU with DMI caching ----
SC_MODULE(DmiCpu) {
    simple_initiator_socket<DmiCpu> socket;

    // Cached DMI descriptor
    bool     dmi_valid;
    tlm_dmi  dmi_data;

    SC_CTOR(DmiCpu) : socket("socket"), dmi_valid(false) {
        socket.register_invalidate_direct_mem_ptr(this,
            &DmiCpu::invalidate_direct_mem_ptr);
        SC_THREAD(run);
    }

    // Called by target when DMI is revoked
    void invalidate_direct_mem_ptr(sc_dt::uint64 start, sc_dt::uint64 end) {
        std::cout << sc_time_stamp()
                  << " CPU: DMI invalidated for range 0x"
                  << std::hex << start << "-0x" << end << std::dec
                  << std::endl;
        dmi_valid = false;
    }

    // Try to get DMI, fall back to b_transport
    void write_word(uint64_t addr, uint32_t data) {
        if (dmi_valid && addr >= dmi_data.get_start_address()
                      && addr + 3 <= dmi_data.get_end_address()) {
            // DMI fast path: direct pointer access!
            unsigned char* ptr = dmi_data.get_dmi_ptr();
            memcpy(&ptr[addr], &data, 4);
            wait(dmi_data.get_write_latency());
            std::cout << sc_time_stamp() << " CPU: DMI WRITE 0x"
                      << std::hex << data << " @ 0x" << addr << std::dec
                      << " (fast path!)" << std::endl;
        } else {
            // Slow path: b_transport
            tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;
            trans.set_command(TLM_WRITE_COMMAND);
            trans.set_address(addr);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
            socket->b_transport(trans, delay);
            wait(delay);

            // Check if target offers DMI
            if (trans.is_dmi_allowed() && !dmi_valid) {
                std::cout << sc_time_stamp()
                          << " CPU: target offers DMI, requesting..."
                          << std::endl;
                dmi_valid = socket->get_direct_mem_ptr(trans, dmi_data);
            }
        }
    }

    uint32_t read_word(uint64_t addr) {
        uint32_t data = 0;
        if (dmi_valid && addr >= dmi_data.get_start_address()
                      && addr + 3 <= dmi_data.get_end_address()) {
            unsigned char* ptr = dmi_data.get_dmi_ptr();
            memcpy(&data, &ptr[addr], 4);
            wait(dmi_data.get_read_latency());
            std::cout << sc_time_stamp() << " CPU: DMI READ  0x"
                      << std::hex << data << " @ 0x" << addr << std::dec
                      << " (fast path!)" << std::endl;
        } else {
            tlm_generic_payload trans;
            sc_time delay = SC_ZERO_TIME;
            trans.set_command(TLM_READ_COMMAND);
            trans.set_address(addr);
            trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
            socket->b_transport(trans, delay);
            wait(delay);
        }
        return data;
    }

    void run() {
        std::cout << "\n--- Phase 1: First access via b_transport ---" << std::endl;
        write_word(0x00, 0x11111111);   // slow — but triggers DMI offer

        std::cout << "\n--- Phase 2: Subsequent accesses via DMI ---" << std::endl;
        write_word(0x04, 0x22222222);   // fast DMI!
        write_word(0x08, 0x33333333);   // fast DMI!
        read_word(0x00);                // fast DMI!
        read_word(0x04);                // fast DMI!

        std::cout << "\n--- Phase 3: DMI invalidated, back to slow path ---"
                  << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 13: TLM-2.0 Direct Memory Interface (DMI) ===" << std::endl;
    std::cout << "DMI = raw pointer to target memory, bypasses b_transport\n"
              << std::endl;

    DmiCpu    cpu("cpu");
    DmiMemory mem("mem");
    cpu.socket.bind(mem.socket);

    sc_start(100, SC_NS);

    // Simulate memory remap — invalidates DMI
    mem.revoke_dmi();

    // CPU's next access will fall back to b_transport
    sc_start(100, SC_NS);

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "get_direct_mem_ptr(): initiator requests raw pointer" << std::endl;
    std::cout << "tlm_dmi: describes valid region, permissions, latency" << std::endl;
    std::cout << "invalidate_direct_mem_ptr(): target revokes access" << std::endl;
    std::cout << "Speedup: avoids socket call overhead per access" << std::endl;

    return 0;
}
