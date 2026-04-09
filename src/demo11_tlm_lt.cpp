// Demo 11: TLM-2.0 Blocking Transport (Loosely Timed)
// =====================================================
// Key concepts:
//   - TLM-2.0 has two coding styles:
//     Loosely Timed (LT): uses b_transport() — one call = full transaction
//     Approximately Timed (AT): uses nb_transport_fw/bw — multi-phase
//   - simple_initiator_socket<T>: port that sends transactions
//   - simple_target_socket<T>: port that receives transactions
//   - tlm_generic_payload: the standard transaction object
//     Fields: command, address, data_ptr, data_length, streaming_width,
//             byte_enable_ptr, response_status
//   - b_transport(trans, delay): blocking call, models full transaction
//   - delay: annotated time — transaction "costs" this much simulated time
//
// This demo: initiator writes/reads a memory, showing each payload field.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

using namespace tlm;
using namespace tlm_utils;

// ---- Memory target (1KB) ----
SC_MODULE(Memory) {
    simple_target_socket<Memory> socket;
    unsigned char mem[1024];

    SC_CTOR(Memory) : socket("socket") {
        socket.register_b_transport(this, &Memory::b_transport);
        memset(mem, 0, sizeof(mem));
    }

    // b_transport: called by the initiator, executes the transaction
    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        // Extract fields from the generic payload
        tlm_command    cmd  = trans.get_command();
        uint64_t       addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int   len  = trans.get_data_length();
        unsigned int   sw   = trans.get_streaming_width();
        unsigned char* byt  = trans.get_byte_enable_ptr();

        // Validate address range
        if (addr + len > sizeof(mem)) {
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        // Validate streaming width (must equal data length for simple access)
        if (sw < len) {
            trans.set_response_status(TLM_BURST_ERROR_RESPONSE);
            return;
        }

        // Execute the command
        if (cmd == TLM_WRITE_COMMAND) {
            if (byt) {
                // Byte enables: only write bytes where enable is 0xFF
                for (unsigned int i = 0; i < len; i++) {
                    if (byt[i] == 0xFF)
                        mem[addr + i] = ptr[i];
                }
            } else {
                memcpy(&mem[addr], ptr, len);
            }
            std::cout << sc_time_stamp() << " Memory: WRITE "
                      << len << "B @ 0x" << std::hex << addr << std::dec
                      << std::endl;
        } else if (cmd == TLM_READ_COMMAND) {
            memcpy(ptr, &mem[addr], len);
            std::cout << sc_time_stamp() << " Memory: READ  "
                      << len << "B @ 0x" << std::hex << addr << std::dec
                      << std::endl;
        } else {
            // TLM_IGNORE_COMMAND — do nothing
            trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        // Annotate delay: model 10ns per access
        delay += sc_time(10, SC_NS);
        trans.set_response_status(TLM_OK_RESPONSE);
    }
};

// ---- CPU initiator ----
SC_MODULE(Cpu) {
    simple_initiator_socket<Cpu> socket;

    // Helper: set up a generic payload and send it
    void do_write(uint64_t addr, unsigned char* data, unsigned int len,
                  sc_time& delay) {
        tlm_generic_payload trans;
        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);  // = data_length for normal access
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, delay);

        if (trans.is_response_error()) {
            std::cout << "  ERROR: " << trans.get_response_string() << std::endl;
        }
    }

    void do_read(uint64_t addr, unsigned char* data, unsigned int len,
                 sc_time& delay) {
        tlm_generic_payload trans;
        trans.set_command(TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, delay);

        if (trans.is_response_error()) {
            std::cout << "  ERROR: " << trans.get_response_string() << std::endl;
        }
    }

    void run() {
        sc_time delay = SC_ZERO_TIME;

        std::cout << "\n--- Basic write/read ---" << std::endl;

        // Write a 4-byte word
        uint32_t wdata = 0xCAFEBEEF;
        do_write(0x00, reinterpret_cast<unsigned char*>(&wdata), 4, delay);
        wait(delay); delay = SC_ZERO_TIME;  // consume annotated delay

        // Read it back
        uint32_t rdata = 0;
        do_read(0x00, reinterpret_cast<unsigned char*>(&rdata), 4, delay);
        wait(delay); delay = SC_ZERO_TIME;

        std::cout << "  Read back: 0x" << std::hex << rdata << std::dec
                  << std::endl;

        // --- Burst write (multiple bytes) ---
        std::cout << "\n--- Burst write (16 bytes) ---" << std::endl;
        unsigned char burst[16];
        for (int i = 0; i < 16; i++) burst[i] = i * 10;
        do_write(0x10, burst, 16, delay);
        wait(delay); delay = SC_ZERO_TIME;

        // Read burst back
        unsigned char rburst[16] = {0};
        do_read(0x10, rburst, 16, delay);
        wait(delay); delay = SC_ZERO_TIME;

        std::cout << "  Read back:";
        for (int i = 0; i < 16; i++) std::cout << " " << (int)rburst[i];
        std::cout << std::endl;

        // --- Byte enables (partial write) ---
        std::cout << "\n--- Byte-enabled write ---" << std::endl;
        unsigned char data4[4] = {0xAA, 0xBB, 0xCC, 0xDD};
        unsigned char mask[4]  = {0xFF, 0x00, 0xFF, 0x00};  // write bytes 0,2 only

        tlm_generic_payload trans;
        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(0x20);
        trans.set_data_ptr(data4);
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(mask);
        trans.set_byte_enable_length(4);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        wait(delay); delay = SC_ZERO_TIME;

        unsigned char check[4] = {0};
        do_read(0x20, check, 4, delay);
        wait(delay); delay = SC_ZERO_TIME;
        std::cout << "  Wrote AA,BB,CC,DD with mask FF,00,FF,00" << std::endl;
        std::cout << "  Read back: "
                  << std::hex << (int)check[0] << "," << (int)check[1]
                  << "," << (int)check[2] << "," << (int)check[3]
                  << std::dec << std::endl;

        // --- Address error ---
        std::cout << "\n--- Address error ---" << std::endl;
        uint32_t dummy = 0;
        do_write(0x3FF, reinterpret_cast<unsigned char*>(&dummy), 4, delay);

        // --- Response status codes ---
        std::cout << "\n--- TLM response status codes ---" << std::endl;
        std::cout << "  TLM_OK_RESPONSE           = success" << std::endl;
        std::cout << "  TLM_INCOMPLETE_RESPONSE    = not yet completed" << std::endl;
        std::cout << "  TLM_ADDRESS_ERROR_RESPONSE = bad address" << std::endl;
        std::cout << "  TLM_COMMAND_ERROR_RESPONSE = unsupported cmd" << std::endl;
        std::cout << "  TLM_BURST_ERROR_RESPONSE   = bad streaming width" << std::endl;
        std::cout << "  TLM_BYTE_ENABLE_ERROR_RESPONSE = bad byte enable" << std::endl;
        std::cout << "  TLM_GENERIC_ERROR_RESPONSE = other error" << std::endl;
    }

    SC_CTOR(Cpu) : socket("socket") { SC_THREAD(run); }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 11: TLM-2.0 Blocking Transport (LT) ===" << std::endl;
    std::cout << "b_transport: one call = one complete transaction" << std::endl;

    Cpu    cpu("cpu");
    Memory mem("mem");
    cpu.socket.bind(mem.socket);  // initiator -> target

    sc_start();
    return 0;
}
