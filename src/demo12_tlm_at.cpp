// Demo 12: TLM-2.0 Non-Blocking Transport (Approximately Timed)
// ================================================================
// Key concepts:
//   - AT (Approximately Timed) uses nb_transport_fw() / nb_transport_bw()
//   - Transactions have PHASES:
//       BEGIN_REQ  → initiator sends request
//       END_REQ    → target acknowledges request
//       BEGIN_RESP → target sends response
//       END_RESP   → initiator acknowledges response
//   - Return value tlm_sync_enum:
//       TLM_ACCEPTED  — phase noted, will call back later
//       TLM_UPDATED   — phase advanced (skip a callback)
//       TLM_COMPLETED — transaction done in one shot
//   - This models pipelined bus protocols (like AXI) more accurately
//   - Transactions can overlap — multiple in-flight at once
//
// This demo: a pipelined initiator and target with 4-phase handshake.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <queue>

using namespace tlm;
using namespace tlm_utils;

// ---- AT Memory Target ----
SC_MODULE(AtMemory) {
    simple_target_socket<AtMemory> socket;
    unsigned char mem[256];

    // Payload Event Queue: schedules callbacks at future times
    // This is the key AT mechanism — events arrive, get delayed, then fire
    peq_with_cb_and_phase<AtMemory> peq;

    SC_CTOR(AtMemory) : socket("socket"), peq(this, &AtMemory::peq_callback) {
        socket.register_nb_transport_fw(this, &AtMemory::nb_transport_fw);
        memset(mem, 0, sizeof(mem));
    }

    // Forward path: initiator -> target
    tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans,
                                   tlm_phase& phase,
                                   sc_time& delay) {
        if (phase == BEGIN_REQ) {
            std::cout << sc_time_stamp() << " Target: received BEGIN_REQ"
                      << " addr=0x" << std::hex << trans.get_address()
                      << std::dec << std::endl;

            // Accept the request, schedule END_REQ after address decode time
            peq.notify(trans, END_REQ, delay + sc_time(5, SC_NS));
            return TLM_ACCEPTED;
        }

        if (phase == END_RESP) {
            std::cout << sc_time_stamp()
                      << " Target: received END_RESP (initiator done)"
                      << std::endl;
            return TLM_COMPLETED;
        }

        return TLM_ACCEPTED;
    }

    // PEQ callback: fires when a scheduled event matures
    void peq_callback(tlm_generic_payload& trans, const tlm_phase& phase) {
        if (phase == END_REQ) {
            std::cout << sc_time_stamp() << " Target: END_REQ — processing"
                      << std::endl;

            // Execute the actual memory operation
            uint64_t       addr = trans.get_address();
            unsigned char* ptr  = trans.get_data_ptr();
            unsigned int   len  = trans.get_data_length();

            if (trans.get_command() == TLM_WRITE_COMMAND) {
                memcpy(&mem[addr], ptr, len);
            } else {
                memcpy(ptr, &mem[addr], len);
            }
            trans.set_response_status(TLM_OK_RESPONSE);

            // Schedule BEGIN_RESP after memory access time
            peq.notify(trans, BEGIN_RESP, sc_time(10, SC_NS));
        }

        if (phase == BEGIN_RESP) {
            std::cout << sc_time_stamp()
                      << " Target: sending BEGIN_RESP" << std::endl;

            // Send response back to initiator via backward path
            tlm_phase resp_phase = BEGIN_RESP;
            sc_time delay = SC_ZERO_TIME;
            socket->nb_transport_bw(trans, resp_phase, delay);
        }
    }
};

// ---- AT CPU Initiator ----
SC_MODULE(AtCpu) {
    simple_initiator_socket<AtCpu> socket;

    sc_event response_event;
    tlm_generic_payload* active_trans;

    SC_CTOR(AtCpu) : socket("socket"), active_trans(nullptr) {
        socket.register_nb_transport_bw(this, &AtCpu::nb_transport_bw);
        SC_THREAD(run);
    }

    // Backward path: target -> initiator (response comes back here)
    tlm_sync_enum nb_transport_bw(tlm_generic_payload& trans,
                                   tlm_phase& phase,
                                   sc_time& delay) {
        if (phase == BEGIN_RESP) {
            std::cout << sc_time_stamp()
                      << " Initiator: received BEGIN_RESP" << std::endl;

            // Acknowledge the response
            tlm_phase ack = END_RESP;
            sc_time d = SC_ZERO_TIME;
            socket->nb_transport_fw(trans, ack, d);

            // Wake up the waiting thread
            response_event.notify();
            return TLM_COMPLETED;
        }
        return TLM_ACCEPTED;
    }

    void do_transaction(tlm_command cmd, uint64_t addr,
                        unsigned char* data, unsigned int len) {
        tlm_generic_payload trans;
        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(data);
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        // Phase 1: BEGIN_REQ
        tlm_phase phase = BEGIN_REQ;
        sc_time delay = SC_ZERO_TIME;

        std::cout << sc_time_stamp() << " Initiator: sending BEGIN_REQ"
                  << " addr=0x" << std::hex << addr << std::dec << std::endl;

        tlm_sync_enum status = socket->nb_transport_fw(trans, phase, delay);

        if (status == TLM_COMPLETED) {
            std::cout << "  Transaction completed immediately" << std::endl;
            return;
        }

        // Wait for response via backward path
        wait(response_event);

        if (trans.is_response_error()) {
            std::cout << "  ERROR: " << trans.get_response_string() << std::endl;
        }
    }

    void run() {
        std::cout << "\n--- AT Write ---" << std::endl;
        uint32_t wdata = 0xDEADC0DE;
        do_transaction(TLM_WRITE_COMMAND, 0x10,
                       reinterpret_cast<unsigned char*>(&wdata), 4);

        wait(5, SC_NS);  // gap between transactions

        std::cout << "\n--- AT Read ---" << std::endl;
        uint32_t rdata = 0;
        do_transaction(TLM_READ_COMMAND, 0x10,
                       reinterpret_cast<unsigned char*>(&rdata), 4);

        std::cout << sc_time_stamp() << " Initiator: read 0x"
                  << std::hex << rdata << std::dec << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 12: TLM-2.0 Non-Blocking Transport (AT) ===" << std::endl;
    std::cout << "4-phase: BEGIN_REQ -> END_REQ -> BEGIN_RESP -> END_RESP\n"
              << std::endl;

    AtCpu    cpu("cpu");
    AtMemory mem("mem");
    cpu.socket.bind(mem.socket);

    sc_start();

    std::cout << "\n--- AT vs LT summary ---" << std::endl;
    std::cout << "LT (b_transport):   simple, one call = done" << std::endl;
    std::cout << "AT (nb_transport):  pipelined, multi-phase, overlapping txns"
              << std::endl;
    std::cout << "Use LT for software development, AT for bus-accurate models"
              << std::endl;

    return 0;
}
