// Demo: TLM-2.0 AT hop-by-hop  PE -> Router -> Router -> Responder
// =================================================================
// Each router forwards BEGIN_REQ downstream and propagates BEGIN_RESP
// back upstream through the return value (TLM_UPDATED).
//
// Phase flow through the chain:
//   PE --BEGIN_REQ--> R1 --BEGIN_REQ--> R2 --BEGIN_REQ--> Responder
//   PE <-BEGIN_RESP-- R1 <-BEGIN_RESP-- R2 <-BEGIN_RESP-- Responder
//   PE --END_RESP---> R1 --END_RESP---> R2 --END_RESP---> Responder

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

using namespace tlm;
using namespace tlm_utils;

// ---- Responder: final destination ----
SC_MODULE(Responder) {
    simple_target_socket<Responder> socket;
    unsigned char mem[256];

    SC_CTOR(Responder) : socket("socket") {
        socket.register_nb_transport_fw(this, &Responder::nb_transport_fw);
        memset(mem, 0, sizeof(mem));
    }

    tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans,
                                   tlm_phase& phase, sc_time& delay) {
        if (phase == BEGIN_REQ) {
            uint64_t       addr = trans.get_address();
            unsigned char* ptr  = trans.get_data_ptr();
            unsigned int   len  = trans.get_data_length();

            if (trans.get_command() == TLM_WRITE_COMMAND)
                memcpy(&mem[addr], ptr, len);
            else
                memcpy(ptr, &mem[addr], len);

            trans.set_response_status(TLM_OK_RESPONSE);

            std::cout << sc_time_stamp() << " Responder: processed "
                      << (trans.get_command() == TLM_WRITE_COMMAND ? "WRITE" : "READ")
                      << " addr=0x" << std::hex << trans.get_address() << std::dec
                      << " delay so far=" << delay << "\n";

            // Skip END_REQ, respond immediately with BEGIN_RESP
            phase = BEGIN_RESP;
            delay += sc_time(5, SC_NS);    // responder access time
            return TLM_UPDATED;
        }

        if (phase == END_RESP) {
            std::cout << sc_time_stamp() << " Responder: END_RESP received\n";
            return TLM_COMPLETED;
        }

        return TLM_ACCEPTED;
    }
};

// ---- Router: adds hop latency and forwards in both directions ----
SC_MODULE(Router) {
    simple_target_socket<Router>    target;
    simple_initiator_socket<Router> initiator;

    std::string label;
    sc_time     hop_latency;

    Router(sc_module_name name, const std::string& lbl, sc_time latency)
        : sc_module(name), target("target"), initiator("initiator"),
          label(lbl), hop_latency(latency) {
        target.register_nb_transport_fw(this, &Router::nb_transport_fw);
    }

    tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans,
                                   tlm_phase& phase, sc_time& delay) {
        if (phase == BEGIN_REQ) {
            delay += hop_latency;    // forward hop cost
            std::cout << sc_time_stamp() << " " << label
                      << ": forwarding BEGIN_REQ, delay now=" << delay << "\n";

            tlm_sync_enum status = initiator->nb_transport_fw(trans, phase, delay);

            if (status == TLM_UPDATED && phase == BEGIN_RESP) {
                // Downstream responded with BEGIN_RESP — propagate it back
                delay += hop_latency;    // return hop cost
                std::cout << sc_time_stamp() << " " << label
                          << ": propagating BEGIN_RESP, delay now=" << delay << "\n";
                return TLM_UPDATED;      // caller sees BEGIN_RESP in phase
            }
            return status;
        }

        if (phase == END_RESP) {
            std::cout << sc_time_stamp() << " " << label
                      << ": forwarding END_RESP\n";
            return initiator->nb_transport_fw(trans, phase, delay);
        }

        return TLM_ACCEPTED;
    }
};

// ---- PE: initiator ----
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

        tlm_phase phase = BEGIN_REQ;
        sc_time   delay = SC_ZERO_TIME;

        std::cout << sc_time_stamp() << " PE: sending BEGIN_REQ\n";
        tlm_sync_enum status = socket->nb_transport_fw(trans, phase, delay);

        if (status == TLM_UPDATED && phase == BEGIN_RESP) {
            // Response came back through the chain
            wait(delay);    // consume total accumulated delay
            std::cout << sc_time_stamp() << " PE: got BEGIN_RESP, total delay="
                      << delay << "\n";

            // Acknowledge with END_RESP
            phase = END_RESP;
            sc_time d = SC_ZERO_TIME;
            socket->nb_transport_fw(trans, phase, d);
        }

        std::cout << sc_time_stamp() << " PE: transaction complete\n\n";
    }

    void run() {
        uint32_t wdata = 0xCAFEBABE;
        std::cout << "--- WRITE ---\n";
        send(TLM_WRITE_COMMAND, 0x10,
             reinterpret_cast<unsigned char*>(&wdata), 4);

        uint32_t rdata = 0;
        std::cout << "--- READ ---\n";
        send(TLM_READ_COMMAND, 0x10,
             reinterpret_cast<unsigned char*>(&rdata), 4);

        std::cout << sc_time_stamp() << " PE: read back 0x"
                  << std::hex << rdata << std::dec << "\n";
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== TLM-2.0 AT: PE -> Router1 -> Router2 -> Responder ===\n\n";

    PE        pe("pe");
    Router    r1("r1", "Router1", sc_time(10, SC_NS));
    Router    r2("r2", "Router2", sc_time(10, SC_NS));
    Responder resp("resp");

    pe.socket.bind(r1.target);
    r1.initiator.bind(r2.target);
    r2.initiator.bind(resp.socket);

    sc_start();
    return 0;
}
