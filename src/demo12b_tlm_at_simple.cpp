// Demo 12b: TLM-2.0 AT — minimal 4-phase handshake, no PEQ
// ==========================================================
// The 4 phases:
//   BEGIN_REQ  — initiator starts a request
//   END_REQ    — target accepts the request
//   BEGIN_RESP — target sends response
//   END_RESP   — initiator acknowledges response
//
// Here the target responds immediately (TLM_UPDATED) skipping END_REQ,
// going straight to BEGIN_RESP. This is the simplest legal AT flow.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

using namespace tlm;
using namespace tlm_utils;

// ---- Simple memory target ----
SC_MODULE(SimpleMemory) {
    simple_target_socket<SimpleMemory> socket;
    unsigned char mem[256];

    SC_CTOR(SimpleMemory) : socket("socket") {
        socket.register_nb_transport_fw(this, &SimpleMemory::nb_transport_fw);
        memset(mem, 0, sizeof(mem));
    }

    tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans,
                                   tlm_phase& phase,
                                   sc_time& delay) {
        if (phase == BEGIN_REQ) {
            std::cout << sc_time_stamp() << " Target: BEGIN_REQ received\n";

            // Execute the transaction immediately
            uint64_t       addr = trans.get_address();
            unsigned char* ptr  = trans.get_data_ptr();
            unsigned int   len  = trans.get_data_length();

            if (trans.get_command() == TLM_WRITE_COMMAND)
                memcpy(&mem[addr], ptr, len);
            else
                memcpy(ptr, &mem[addr], len);

            trans.set_response_status(TLM_OK_RESPONSE);

            // Skip END_REQ, go straight to BEGIN_RESP
            phase = BEGIN_RESP;
            delay = sc_time(10, SC_NS);  // response arrives in 10ns
            std::cout << sc_time_stamp() << " Target: returning BEGIN_RESP\n";
            return TLM_UPDATED;          // tells initiator: phase has changed
        }

        if (phase == END_RESP) {
            std::cout << sc_time_stamp() << " Target: END_RESP received, done\n";
            return TLM_COMPLETED;
        }

        return TLM_ACCEPTED;
    }
};

// ---- Simple CPU initiator ----
SC_MODULE(SimpleCpu) {
    simple_initiator_socket<SimpleCpu> socket;
    sc_event response_event;

    SC_CTOR(SimpleCpu) : socket("socket") {
        socket.register_nb_transport_bw(this, &SimpleCpu::nb_transport_bw);
        SC_THREAD(run);
    }

    // Backward path: target calls this to deliver BEGIN_RESP
    tlm_sync_enum nb_transport_bw(tlm_generic_payload& trans,
                                   tlm_phase& phase,
                                   sc_time& delay) {
        if (phase == BEGIN_RESP) {
            std::cout << sc_time_stamp() << " Initiator: BEGIN_RESP received\n";

            // Acknowledge with END_RESP
            phase = END_RESP;
            sc_time d = SC_ZERO_TIME;
            socket->nb_transport_fw(trans, phase, d);

            response_event.notify();  // wake up run()
            return TLM_COMPLETED;
        }
        return TLM_ACCEPTED;
    }

    void run() {
        // --- Write ---
        uint32_t wdata = 0xCAFEBABE;
        tlm_generic_payload trans;
        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(0x10);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&wdata));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        tlm_phase phase = BEGIN_REQ;
        sc_time   delay = SC_ZERO_TIME;

        std::cout << sc_time_stamp() << " Initiator: sending BEGIN_REQ (write)\n";
        tlm_sync_enum status = socket->nb_transport_fw(trans, phase, delay);

        if (status == TLM_UPDATED && phase == BEGIN_RESP) {
            // Target skipped END_REQ and returned BEGIN_RESP directly
            wait(delay);                        // wait the annotated response delay
            nb_transport_bw(trans, phase, delay); // handle it as if it came backwards
        } else {
            wait(response_event);               // wait for backward callback
        }

        // --- Read back ---
        uint32_t rdata = 0;
        trans.set_command(TLM_READ_COMMAND);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&rdata));
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        phase = BEGIN_REQ;
        delay = SC_ZERO_TIME;

        std::cout << sc_time_stamp() << " Initiator: sending BEGIN_REQ (read)\n";
        status = socket->nb_transport_fw(trans, phase, delay);

        if (status == TLM_UPDATED && phase == BEGIN_RESP) {
            wait(delay);
            nb_transport_bw(trans, phase, delay);
        } else {
            wait(response_event);
        }

        std::cout << sc_time_stamp() << " Initiator: read back 0x"
                  << std::hex << rdata << std::dec << "\n";
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 12b: Minimal AT 4-phase handshake ===\n\n";

    SimpleCpu    cpu("cpu");
    SimpleMemory mem("mem");
    cpu.socket.bind(mem.socket);

    sc_start();
    return 0;
}
