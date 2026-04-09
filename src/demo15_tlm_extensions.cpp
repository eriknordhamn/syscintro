// Demo 15: TLM-2.0 Extensions and Temporal Decoupling
// =====================================================
// Key concepts:
//   - tlm_extension<T>: attach custom data to a generic payload
//     (e.g., cache attributes, security bits, QoS, transaction ID)
//   - Extensions let you add protocol-specific info without changing
//     the generic payload — keeps interoperability
//   - Temporal decoupling: initiator runs ahead of simulation time
//     using a local time offset (quantum), then syncs periodically
//   - tlm_quantumkeeper: manages the local time offset
//     - set_global_quantum(): how far ahead a process can run
//     - inc(): add time to local offset
//     - need_sync(): true when local time exceeds quantum
//     - sync(): synchronize with SystemC kernel (calls wait())
//   - Huge speedup: fewer context switches between processes
//
// This demo: CPU with cache-attribute extensions + temporal decoupling.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/tlm_quantumkeeper.h>

using namespace tlm;
using namespace tlm_utils;

// ---- Custom extension: cache attributes ----
// To create an extension:
//   1. Inherit from tlm_extension<YourClass>
//   2. Implement clone() and copy_from()
struct CacheExtension : public tlm_extension<CacheExtension> {
    enum CachePolicy { UNCACHED, WRITE_THROUGH, WRITE_BACK };

    CachePolicy policy;
    bool        allocate;    // allocate on miss?
    int         cache_line;  // which cache line

    CacheExtension()
        : policy(UNCACHED), allocate(false), cache_line(-1) {}

    // Required: clone for autoptr management
    tlm_extension_base* clone() const override {
        CacheExtension* ext = new CacheExtension();
        ext->policy = policy;
        ext->allocate = allocate;
        ext->cache_line = cache_line;
        return ext;
    }

    // Required: copy data from another extension
    void copy_from(const tlm_extension_base& other) override {
        const CacheExtension& o = static_cast<const CacheExtension&>(other);
        policy = o.policy;
        allocate = o.allocate;
        cache_line = o.cache_line;
    }

    const char* policy_str() const {
        switch (policy) {
            case UNCACHED:      return "UNCACHED";
            case WRITE_THROUGH: return "WRITE_THROUGH";
            case WRITE_BACK:    return "WRITE_BACK";
            default:            return "UNKNOWN";
        }
    }
};

// ---- Custom extension: transaction ID ----
struct TxIdExtension : public tlm_extension<TxIdExtension> {
    uint64_t tx_id;
    int      priority;  // 0=low, 3=high

    TxIdExtension() : tx_id(0), priority(0) {}

    tlm_extension_base* clone() const override {
        TxIdExtension* ext = new TxIdExtension();
        ext->tx_id = tx_id;
        ext->priority = priority;
        return ext;
    }

    void copy_from(const tlm_extension_base& other) override {
        const TxIdExtension& o = static_cast<const TxIdExtension&>(other);
        tx_id = o.tx_id;
        priority = o.priority;
    }
};

// ---- Memory that reads extensions ----
SC_MODULE(ExtMemory) {
    simple_target_socket<ExtMemory> socket;
    unsigned char mem[256];

    SC_CTOR(ExtMemory) : socket("socket") {
        socket.register_b_transport(this, &ExtMemory::b_transport);
        memset(mem, 0, sizeof(mem));
    }

    void b_transport(tlm_generic_payload& trans, sc_time& delay) {
        uint64_t       addr = trans.get_address();
        unsigned char* ptr  = trans.get_data_ptr();
        unsigned int   len  = trans.get_data_length();

        // Check for cache extension
        CacheExtension* cache_ext = trans.get_extension<CacheExtension>();
        if (cache_ext) {
            std::cout << sc_time_stamp() << " Memory: cache policy="
                      << cache_ext->policy_str()
                      << " allocate=" << cache_ext->allocate
                      << " line=" << cache_ext->cache_line << std::endl;

            // Model different latencies based on cache policy
            switch (cache_ext->policy) {
                case CacheExtension::WRITE_BACK:
                    delay += sc_time(5, SC_NS);   // fast
                    break;
                case CacheExtension::WRITE_THROUGH:
                    delay += sc_time(15, SC_NS);  // medium
                    break;
                default:
                    delay += sc_time(30, SC_NS);  // slow
            }
        } else {
            delay += sc_time(10, SC_NS);  // default
        }

        // Check for transaction ID extension
        TxIdExtension* id_ext = trans.get_extension<TxIdExtension>();
        if (id_ext) {
            std::cout << sc_time_stamp() << " Memory: tx_id="
                      << id_ext->tx_id << " priority=" << id_ext->priority
                      << std::endl;
        }

        // Execute
        if (trans.get_command() == TLM_WRITE_COMMAND)
            memcpy(&mem[addr], ptr, len);
        else
            memcpy(ptr, &mem[addr], len);

        trans.set_response_status(TLM_OK_RESPONSE);
    }
};

// ---- CPU with temporal decoupling ----
SC_MODULE(ExtCpu) {
    simple_initiator_socket<ExtCpu> socket;
    tlm_quantumkeeper qk;  // manages local time offset

    uint64_t next_tx_id;

    SC_CTOR(ExtCpu) : socket("socket"), next_tx_id(0) {
        SC_THREAD(run);
    }

    void do_write(uint64_t addr, uint32_t data,
                  CacheExtension::CachePolicy policy) {
        tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(TLM_INCOMPLETE_RESPONSE);

        // Attach cache extension
        CacheExtension* cache_ext = new CacheExtension();
        cache_ext->policy = policy;
        cache_ext->allocate = (policy != CacheExtension::UNCACHED);
        cache_ext->cache_line = (addr >> 5) & 0xF;  // 32B lines
        trans.set_extension(cache_ext);

        // Attach transaction ID extension
        TxIdExtension* id_ext = new TxIdExtension();
        id_ext->tx_id = next_tx_id++;
        id_ext->priority = (policy == CacheExtension::WRITE_BACK) ? 3 : 1;
        trans.set_extension(id_ext);

        // Call b_transport
        socket->b_transport(trans, delay);

        // Temporal decoupling: accumulate delay instead of calling wait()
        qk.inc(delay);

        // Only sync when we've run too far ahead
        if (qk.need_sync()) {
            std::cout << sc_time_stamp() << " CPU: quantum exceeded, syncing"
                      << " (local time was +" << qk.get_local_time() << ")"
                      << std::endl;
            qk.sync();  // calls wait(), advances simulation time
        }

        // Clean up extensions
        trans.clear_extension(cache_ext);
        trans.clear_extension(id_ext);
        delete cache_ext;
        delete id_ext;
    }

    void run() {
        // Set quantum: how far ahead we can run (100ns)
        qk.set_global_quantum(sc_time(100, SC_NS));
        qk.reset();

        std::cout << "\n--- Extensions demo ---" << std::endl;
        std::cout << "Global quantum: " << tlm_quantumkeeper::get_global_quantum()
                  << std::endl;

        // Write with different cache policies
        std::cout << "\n--- Uncached write ---" << std::endl;
        do_write(0x00, 0x11, CacheExtension::UNCACHED);

        std::cout << "\n--- Write-through write ---" << std::endl;
        do_write(0x04, 0x22, CacheExtension::WRITE_THROUGH);

        std::cout << "\n--- Write-back writes (fast, will accumulate) ---"
                  << std::endl;
        for (int i = 0; i < 10; i++) {
            do_write(0x10 + i*4, i, CacheExtension::WRITE_BACK);
        }

        std::cout << "\n--- Final sync ---" << std::endl;
        std::cout << sc_time_stamp() << " CPU: local time offset = "
                  << qk.get_local_time() << std::endl;
        qk.sync();
        std::cout << sc_time_stamp() << " CPU: synced, done" << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 15: TLM Extensions & Temporal Decoupling ===" << std::endl;

    ExtCpu    cpu("cpu");
    ExtMemory mem("mem");
    cpu.socket.bind(mem.socket);

    sc_start();

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "tlm_extension<T>:" << std::endl;
    std::cout << "  - Attach custom data to any generic payload" << std::endl;
    std::cout << "  - Must implement clone() and copy_from()" << std::endl;
    std::cout << "  - set_extension() / get_extension<T>() / clear_extension()"
              << std::endl;
    std::cout << "\ntlm_quantumkeeper:" << std::endl;
    std::cout << "  - Initiator runs ahead without calling wait()" << std::endl;
    std::cout << "  - inc(delay): accumulate local time" << std::endl;
    std::cout << "  - need_sync(): check if quantum exceeded" << std::endl;
    std::cout << "  - sync(): call wait() to realign with kernel" << std::endl;
    std::cout << "  - Fewer context switches = faster simulation" << std::endl;

    return 0;
}
