// Demo 17: Credit-based flow control over TLM-2.0 nb_transport (4-phase)
// ========================================================================
// Key concepts:
//   - Credit-based flow control: sender can only transmit if it has credits
//   - Target has N buffer slots; sender starts with N credits
//   - Each BEGIN_REQ consumes 1 credit; target returns credits as it drains
//   - This prevents buffer overflow WITHOUT lossy drops or retries
//   - Common in NoCs, PCIe, CXL, HBM, CCIX — any credit-managed fabric
//
//   - Implemented with the TLM-2.0 AT-style 4 phases:
//       BEGIN_REQ  (initiator -> target) : "new transaction, consumes a credit"
//       END_REQ    (target -> initiator) : "buffered, you may send another"
//                                         (N.B. we use END_REQ as the credit-
//                                         return point — target tells the
//                                         sender a slot is freed)
//       BEGIN_RESP (target -> initiator) : "response ready"
//       END_RESP   (initiator -> target) : "response consumed, done"
//
//   - Key design choice: credit returns are tied to END_REQ (slot freed
//     when target buffer entry becomes available). In real fabrics the
//     credit return can be independent; here we piggyback it on END_REQ.
//
//   - The initiator uses an sc_event to block when credits == 0 and to
//     wake up when a credit is returned.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <queue>

using namespace tlm;
using namespace tlm_utils;

static const int TARGET_BUFFER_DEPTH = 3;   // target has 3 buffer slots
static const int NUM_TRANSACTIONS    = 8;   // initiator sends 8 transactions

// ---- Target with finite buffer and credit accounting ----
SC_MODULE(CreditTarget) {
    simple_target_socket<CreditTarget> socket;

    // Internal buffer of pending transactions
    std::queue<tlm_generic_payload*> buffer;
    int buffer_depth;

    // Event queue to schedule phase transitions at future simulated times
    peq_with_cb_and_phase<CreditTarget> peq;

    // Payload storage (owned by target so we can process them later)
    unsigned char storage[256];

    SC_HAS_PROCESS(CreditTarget);
    CreditTarget(sc_module_name name, int depth)
        : sc_module(name), socket("socket"), buffer_depth(depth),
          peq(this, &CreditTarget::peq_callback)
    {
        memset(storage, 0, sizeof(storage));
        socket.register_nb_transport_fw(this, &CreditTarget::nb_transport_fw);
        SC_THREAD(process_buffer);
    }

    // Forward path: initiator -> target
    tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans,
                                   tlm_phase& phase,
                                   sc_time& delay) {
        if (phase == BEGIN_REQ) {
            // Sanity check: initiator should never send when no credits,
            // but enforce here anyway.
            if ((int)buffer.size() >= buffer_depth) {
                std::cout << sc_time_stamp() << " Target: OVERFLOW! "
                          << "initiator violated credit protocol" << std::endl;
                trans.set_response_status(TLM_GENERIC_ERROR_RESPONSE);
                return TLM_COMPLETED;
            }

            // Store the transaction in our buffer (consumes one slot)
            buffer.push(&trans);
            std::cout << sc_time_stamp() << " Target: BEGIN_REQ buffered "
                      << " (buf=" << buffer.size() << "/" << buffer_depth
                      << ")" << std::endl;

            // Schedule END_REQ after a small acceptance delay.
            // END_REQ is where we return the credit: "slot freed logically"
            // actually we return credit AFTER processing. For this demo:
            //   END_REQ = "request phase complete, but slot still used"
            //   credit return happens when processing completes.
            //
            // To keep the example clear we return the credit at END_REQ
            // timing-wise, but only AFTER the buffered transaction starts
            // being processed (see process_buffer thread).
            peq.notify(trans, END_REQ, delay + sc_time(2, SC_NS));

            // Wake the processing thread
            process_event.notify(SC_ZERO_TIME);

            return TLM_ACCEPTED;
        }

        if (phase == END_RESP) {
            std::cout << sc_time_stamp()
                      << " Target: END_RESP (initiator ack'd response)"
                      << std::endl;
            return TLM_COMPLETED;
        }

        return TLM_ACCEPTED;
    }

    sc_event process_event;

    // Processes buffered transactions one at a time.
    void process_buffer() {
        while (true) {
            if (buffer.empty()) {
                wait(process_event);
                continue;
            }

            // Work on the front transaction
            tlm_generic_payload* trans = buffer.front();
            std::cout << sc_time_stamp() << " Target: processing pending txn"
                      << std::endl;

            // Model processing delay
            wait(15, SC_NS);

            // Execute the transaction
            uint64_t       addr = trans->get_address();
            unsigned char* ptr  = trans->get_data_ptr();
            unsigned int   len  = trans->get_data_length();
            if (trans->get_command() == TLM_WRITE_COMMAND)
                memcpy(&storage[addr], ptr, len);
            else
                memcpy(ptr, &storage[addr], len);
            trans->set_response_status(TLM_OK_RESPONSE);

            // Pop from buffer — slot is now free. Credit will be returned
            // to the initiator via END_REQ on the backward path.
            buffer.pop();

            std::cout << sc_time_stamp() << " Target: slot freed, returning credit"
                      << " (buf=" << buffer.size() << "/" << buffer_depth
                      << ")" << std::endl;

            // Return credit: send END_REQ on backward path
            // (normally END_REQ is the target's acceptance; here we re-use
            // it as our credit-return signal)
            tlm_phase phase = END_REQ;
            sc_time   d = SC_ZERO_TIME;
            socket->nb_transport_bw(*trans, phase, d);

            // Now send the response
            wait(5, SC_NS);
            phase = BEGIN_RESP;
            d = SC_ZERO_TIME;
            std::cout << sc_time_stamp() << " Target: sending BEGIN_RESP"
                      << std::endl;
            socket->nb_transport_bw(*trans, phase, d);
        }
    }

    // PEQ callback (not used for credit logic here, but kept for reference)
    void peq_callback(tlm_generic_payload& trans, const tlm_phase& phase) {
        // Reserved — could be used to model acceptance delay
    }
};

// ---- Initiator with credit counter ----
SC_MODULE(CreditInitiator) {
    simple_initiator_socket<CreditInitiator> socket;

    int credits;            // available credits
    sc_event credit_event;  // fired when a credit is returned
    int outstanding;
    int completed;

    SC_HAS_PROCESS(CreditInitiator);
    CreditInitiator(sc_module_name name, int initial_credits)
        : sc_module(name), socket("socket"),
          credits(initial_credits), outstanding(0), completed(0)
    {
        socket.register_nb_transport_bw(this, &CreditInitiator::nb_transport_bw);
        SC_THREAD(run);
    }

    // Backward path: target -> initiator
    tlm_sync_enum nb_transport_bw(tlm_generic_payload& trans,
                                   tlm_phase& phase,
                                   sc_time& delay) {
        if (phase == END_REQ) {
            // Credit return!
            credits++;
            std::cout << sc_time_stamp() << " Initiator: END_REQ — credit returned"
                      << " (credits=" << credits << ")" << std::endl;
            credit_event.notify(SC_ZERO_TIME);
            return TLM_ACCEPTED;
        }

        if (phase == BEGIN_RESP) {
            std::cout << sc_time_stamp() << " Initiator: BEGIN_RESP received"
                      << std::endl;
            completed++;
            outstanding--;

            // Acknowledge the response
            tlm_phase ack = END_RESP;
            sc_time d = SC_ZERO_TIME;
            socket->nb_transport_fw(trans, ack, d);
            return TLM_COMPLETED;
        }

        return TLM_ACCEPTED;
    }

    void send_transaction(int idx) {
        // Check credits — block if none available
        while (credits <= 0) {
            std::cout << sc_time_stamp() << " Initiator: NO CREDITS, blocking..."
                      << std::endl;
            wait(credit_event);
            std::cout << sc_time_stamp() << " Initiator: woke up (credits="
                      << credits << ")" << std::endl;
        }

        credits--;
        outstanding++;

        // Build a transaction
        tlm_generic_payload* trans = new tlm_generic_payload();
        uint32_t* data = new uint32_t(0xA000 + idx);
        trans->set_command(TLM_WRITE_COMMAND);
        trans->set_address(idx * 4);
        trans->set_data_ptr(reinterpret_cast<unsigned char*>(data));
        trans->set_data_length(4);
        trans->set_streaming_width(4);
        trans->set_byte_enable_ptr(nullptr);
        trans->set_dmi_allowed(false);
        trans->set_response_status(TLM_INCOMPLETE_RESPONSE);

        std::cout << sc_time_stamp() << " Initiator: sending txn#" << idx
                  << " (credits=" << credits << " outstanding="
                  << outstanding << ")" << std::endl;

        tlm_phase phase = BEGIN_REQ;
        sc_time   delay = SC_ZERO_TIME;
        socket->nb_transport_fw(*trans, phase, delay);
    }

    void run() {
        std::cout << "\n=== Credit-based flow control demo ===" << std::endl;
        std::cout << "Target buffer depth: " << TARGET_BUFFER_DEPTH << std::endl;
        std::cout << "Initial credits:     " << credits << std::endl;
        std::cout << "Transactions to send: " << NUM_TRANSACTIONS << "\n"
                  << std::endl;

        // Fire off transactions as fast as credits allow
        for (int i = 0; i < NUM_TRANSACTIONS; i++) {
            send_transaction(i);
            wait(1, SC_NS);  // tiny gap to make trace readable
        }

        // Wait for all to drain
        while (outstanding > 0) {
            wait(10, SC_NS);
        }

        std::cout << "\n" << sc_time_stamp()
                  << " Initiator: all " << completed << " transactions completed"
                  << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 17: Credit-based Flow Control (TLM nb_transport) ==="
              << std::endl;

    CreditInitiator cpu("cpu", TARGET_BUFFER_DEPTH);
    CreditTarget    mem("mem", TARGET_BUFFER_DEPTH);
    cpu.socket.bind(mem.socket);

    sc_start();

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Credit-based flow control:" << std::endl;
    std::cout << "  1. Initiator starts with N credits = target buffer depth"
              << std::endl;
    std::cout << "  2. BEGIN_REQ consumes a credit" << std::endl;
    std::cout << "  3. Target returns credit (END_REQ on backward path)"
              << std::endl;
    std::cout << "     when it frees a buffer slot" << std::endl;
    std::cout << "  4. Initiator blocks on credit_event when credits == 0"
              << std::endl;
    std::cout << "  5. No drops, no retries — buffer can never overflow"
              << std::endl;
    std::cout << "\nReal-world examples:" << std::endl;
    std::cout << "  - PCIe posted/non-posted/completion credits" << std::endl;
    std::cout << "  - CXL flit credits" << std::endl;
    std::cout << "  - On-chip NoC VC credits" << std::endl;
    std::cout << "  - InfiniBand link-level flow control" << std::endl;

    return 0;
}
