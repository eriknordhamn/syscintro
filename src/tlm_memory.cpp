#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

using namespace tlm;
using namespace tlm_utils;

// ---------------- Target: 256-byte memory ----------------
SC_MODULE(Memory) {
    simple_target_socket<Memory> socket;
    unsigned char mem[256];

    SC_CTOR(Memory) : socket("socket") {
        socket.register_b_transport(this, &Memory::b_transport);
        memset(mem, 0, sizeof(mem));
    }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        tlm_command cmd = trans.get_command();
        uint64_t    adr = trans.get_address();
        unsigned char* ptr = trans.get_data_ptr();
        unsigned int   len = trans.get_data_length();

        if (adr + len > sizeof(mem)) {
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (cmd == TLM_WRITE_COMMAND) memcpy(&mem[adr], ptr, len);
        else if (cmd == TLM_READ_COMMAND) memcpy(ptr, &mem[adr], len);

        delay += sc_time(10, SC_NS);              // model latency
        trans.set_response_status(TLM_OK_RESPONSE);
    }
};

// ---------------- Initiator ----------------
SC_MODULE(Cpu) {
    simple_initiator_socket<Cpu> socket;

    SC_CTOR(Cpu) : socket("socket") {
        SC_THREAD(run);
    }

    void run() {
        uint32_t data = 0xDEADBEEF;
        tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        // Write
        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(0x10);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, delay);
        if (trans.is_response_error()) SC_REPORT_ERROR("CPU", "write failed");
        wait(delay); delay = SC_ZERO_TIME;

        // Read back
        uint32_t rdata = 0;
        trans.set_command(TLM_READ_COMMAND);
        trans.set_address(0x10);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&rdata));
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, delay);
        wait(delay);

        std::cout << "@" << sc_time_stamp()
                  << " read 0x" << std::hex << rdata << std::endl;
    }
};

// ---------------- Top ----------------
SC_MODULE(Top) {
    Cpu    cpu;
    Memory mem;
    SC_CTOR(Top) : cpu("cpu"), mem("mem") {
        cpu.socket.bind(mem.socket);
    }
};

int sc_main(int, char*[]) {
    Top top("top");
    sc_start();
    return 0;
}
