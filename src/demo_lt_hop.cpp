// Demo: TLM-2.0 LT hop-by-hop  PE -> Router -> Router -> Responder
// =================================================================
// Each router has a target socket (receives) and an initiator socket (forwards).
// The packet travels as a tlm_generic_payload through each hop.
// Each router adds its own latency to the delay.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

using namespace tlm;
using namespace tlm_utils;

// ---- Responder: final destination, acts like a memory ----
SC_MODULE(Responder) {
    simple_target_socket<Responder> socket;
    unsigned char mem[256];

    SC_CTOR(Responder) : socket("socket") {
        socket.register_b_transport(this, &Responder::b_transport);
        memset(mem, 0, sizeof(mem));
    }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        uint64_t       addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int   len  = trans.get_data_length();

        if (trans.get_command() == TLM_WRITE_COMMAND)
            memcpy(&mem[addr], ptr, len);
        else
            memcpy(ptr, &mem[addr], len);

        std::cout << sc_time_stamp() << " Responder: "
                  << (trans.get_command() == TLM_WRITE_COMMAND ? "WRITE" : "READ")
                  << " addr=0x" << std::hex << addr << std::dec
                  << " (accumulated delay=" << delay << ")\n";

        delay += sc_time(5, SC_NS);   // responder access time
        trans.set_response_status(TLM_OK_RESPONSE);
    }
};

// ---- Router: forwards the transaction and adds hop latency ----
SC_MODULE(Router) {
    simple_target_socket<Router>    target;   // receives from upstream
    simple_initiator_socket<Router> initiator; // forwards downstream

    std::string label;
    sc_time     hop_latency;

    Router(sc_module_name name, const std::string& lbl, sc_time latency)
        : sc_module(name), target("target"), initiator("initiator"),
          label(lbl), hop_latency(latency) {
        target.register_b_transport(this, &Router::b_transport);
    }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        std::cout << sc_time_stamp() << " " << label
                  << ": forwarding, adding hop latency " << hop_latency << "\n";

        delay += hop_latency;             // add this hop's cost to the delay
        initiator->b_transport(trans, delay); // forward downstream
    }
};

// ---- PE: sends a write then reads it back ----
SC_MODULE(PE) {
    simple_initiator_socket<PE> socket;

    SC_CTOR(PE) : socket("socket") {
        SC_THREAD(run);
    }

    void send(tlm_command cmd, uint64_t addr,
              unsigned char* data, unsigned int len) {
        tlm_generic_payload trans;
        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        sc_time delay = SC_ZERO_TIME;
        socket->b_transport(trans, delay);  // travels: PE->R1->R2->Responder
        wait(delay);                        // consume total accumulated delay

        std::cout << sc_time_stamp() << " PE: transaction done, total delay was "
                  << delay << "\n\n";
    }

    void run() {
        uint32_t wdata = 0xDEADBEEF;
        std::cout << sc_time_stamp() << " PE: sending WRITE\n";
        send(TLM_WRITE_COMMAND, 0x10,
             reinterpret_cast<unsigned char*>(&wdata), 4);

        uint32_t rdata = 0;
        std::cout << sc_time_stamp() << " PE: sending READ\n";
        send(TLM_READ_COMMAND, 0x10,
             reinterpret_cast<unsigned char*>(&rdata), 4);

        std::cout << sc_time_stamp() << " PE: read back 0x"
                  << std::hex << rdata << std::dec << "\n";
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== TLM-2.0 LT: PE -> Router1 -> Router2 -> Responder ===\n\n";

    PE        pe("pe");
    Router    r1("r1", "Router1", sc_time(10, SC_NS));
    Router    r2("r2", "Router2", sc_time(10, SC_NS));
    Responder resp("resp");

    // Wire up the chain
    pe.socket.bind(r1.target);
    r1.initiator.bind(r2.target);
    r2.initiator.bind(resp.socket);

    sc_start();
    return 0;
}
