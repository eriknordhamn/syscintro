// Demo 16: TLM-1.0 channels — tlm_fifo, tlm_analysis_port, tlm_req_rsp_channel
// ================================================================================
// Key concepts:
//   - tlm_fifo<T>: typed FIFO with TLM put/get/peek interfaces
//     - put(): blocking write (like sc_fifo::write)
//     - get(): blocking read + remove
//     - peek(): blocking read without removing
//     - nb_put/nb_get/nb_peek: non-blocking variants
//     - nb_can_put/nb_can_get: check availability
//   - tlm_analysis_port<T>: 1-to-N broadcast, zero-time delivery
//     - write(): calls all subscribers immediately
//     - Subscribers implement tlm_analysis_if<T>::write()
//     - Used for monitors, scoreboards, logging
//   - tlm_req_rsp_channel<REQ,RSP>: paired put/get FIFOs
//     - Initiator puts request, gets response
//     - Target gets request, puts response
//   - These are TLM-1.0 constructs — still widely used alongside TLM-2.0 sockets
//
// This demo: a packet processor pipeline with analysis monitoring.

#include <systemc.h>
#include <tlm.h>
#include <tlm_utils/tlm_quantumkeeper.h>

using namespace tlm;

// ---- A simple transaction type ----
struct Packet {
    int id;
    int src;
    int dst;
    int payload;

    Packet() : id(0), src(0), dst(0), payload(0) {}
    Packet(int i, int s, int d, int p) : id(i), src(s), dst(d), payload(p) {}

    friend std::ostream& operator<<(std::ostream& os, const Packet& p) {
        os << "[Pkt#" << p.id << " " << p.src << "->" << p.dst
           << " data=" << p.payload << "]";
        return os;
    }

    bool operator==(const Packet& o) const {
        return id == o.id && src == o.src && dst == o.dst && payload == o.payload;
    }
};

// ========================================
// Part A: tlm_fifo<T>
// ========================================

// ---- Producer: puts packets into a tlm_fifo ----
SC_MODULE(FifoProducer) {
    sc_port<tlm_blocking_put_if<Packet>> out;
    // sc_port<tlm_blocking_put_if<T>> connects to any channel
    // that implements blocking put — including tlm_fifo

    void run() {
        for (int i = 0; i < 6; i++) {
            Packet pkt(i, 1, 2, i * 100);
            std::cout << sc_time_stamp() << " Producer: put " << pkt << std::endl;

            out->put(pkt);   // BLOCKS if fifo is full

            std::cout << sc_time_stamp() << " Producer: put accepted" << std::endl;
            wait(5, SC_NS);
        }
    }

    SC_CTOR(FifoProducer) { SC_THREAD(run); }
};

// ---- Consumer: gets packets, demonstrates peek vs get ----
SC_MODULE(FifoConsumer) {
    sc_port<tlm_blocking_get_peek_if<Packet>> in;
    // tlm_blocking_get_peek_if combines get() and peek()

    void run() {
        while (true) {
            // peek: look at front without removing
            Packet peeked;
            in->peek(peeked);
            std::cout << sc_time_stamp() << " Consumer: peeked " << peeked
                      << std::endl;

            // get: remove from fifo
            Packet got;
            in->get(got);
            std::cout << sc_time_stamp() << " Consumer: got    " << got
                      << std::endl;

            wait(15, SC_NS);  // slow consumer — will cause backpressure
        }
    }

    SC_CTOR(FifoConsumer) { SC_THREAD(run); }
};

// ---- Non-blocking checker ----
SC_MODULE(FifoChecker) {
    sc_port<tlm_nonblocking_get_if<Packet>> in;

    void run() {
        wait(80, SC_NS);  // start after main demo

        std::cout << "\n--- Non-blocking get ---" << std::endl;
        Packet p;
        while (in->nb_get(p)) {
            std::cout << sc_time_stamp() << " Checker: nb_get " << p << std::endl;
        }
        std::cout << sc_time_stamp() << " Checker: fifo empty, nb_get returned false"
                  << std::endl;
    }

    SC_CTOR(FifoChecker) { SC_THREAD(run); }
};

// ========================================
// Part B: tlm_analysis_port<T>
// ========================================

// ---- Transaction source that broadcasts via analysis port ----
SC_MODULE(TxSource) {
    tlm_analysis_port<Packet> analysis_out;
    // analysis_port: write() broadcasts to ALL bound subscribers
    // No blocking — all subscribers called immediately in same delta

    void run() {
        wait(120, SC_NS);  // start after Part A

        std::cout << "\n========== Part B: tlm_analysis_port ==========" << std::endl;
        std::cout << "Broadcasting to all subscribers simultaneously\n" << std::endl;

        for (int i = 0; i < 4; i++) {
            Packet pkt(100 + i, 5, 10, i * 50);
            std::cout << sc_time_stamp() << " Source: broadcasting " << pkt
                      << std::endl;

            analysis_out.write(pkt);  // all subscribers see this instantly

            wait(10, SC_NS);
        }
    }

    SC_CTOR(TxSource) : analysis_out("analysis_out") { SC_THREAD(run); }
};

// ---- Monitor: subscriber that logs transactions ----
SC_MODULE(Monitor) {
    // To subscribe, implement tlm_analysis_if<T>
    // which requires: void write(const T&)

    std::string label;
    int count;

    // Use an export + internal implementation
    tlm_analysis_if<Packet>* get_if() { return &writer; }

    struct Writer : public tlm_analysis_if<Packet> {
        Monitor* parent;
        Writer(Monitor* p) : parent(p) {}

        void write(const Packet& pkt) override {
            parent->count++;
            std::cout << sc_time_stamp() << " " << parent->label
                      << ": observed " << pkt
                      << " (total=" << parent->count << ")" << std::endl;
        }
    } writer;

    SC_HAS_PROCESS(Monitor);
    Monitor(sc_module_name name, const char* lbl)
        : sc_module(name), label(lbl), count(0), writer(this) {}
};

// ---- Scoreboard: subscriber that checks correctness ----
SC_MODULE(Scoreboard) {
    int expected_id;

    struct Writer : public tlm_analysis_if<Packet> {
        Scoreboard* parent;
        Writer(Scoreboard* p) : parent(p) {}

        void write(const Packet& pkt) override {
            if (pkt.id == parent->expected_id) {
                std::cout << sc_time_stamp()
                          << " Scoreboard: PASS pkt#" << pkt.id << std::endl;
            } else {
                std::cout << sc_time_stamp()
                          << " Scoreboard: FAIL expected#"
                          << parent->expected_id << " got#" << pkt.id
                          << std::endl;
            }
            parent->expected_id++;
        }
    } writer;

    SC_HAS_PROCESS(Scoreboard);
    Scoreboard(sc_module_name name)
        : sc_module(name), expected_id(100), writer(this) {}
};

// ========================================
// Part C: tlm_req_rsp_channel<REQ,RSP>
// ========================================

struct Request {
    int id;
    int addr;
    bool is_write;
    int data;
};

struct Response {
    int id;
    int data;
    bool ok;
};

SC_MODULE(Master) {
    sc_port<tlm_blocking_put_if<Request>>  req_out;
    sc_port<tlm_blocking_get_if<Response>> rsp_in;

    void run() {
        wait(200, SC_NS);  // start after Part B

        std::cout << "\n========== Part C: tlm_req_rsp_channel ==========" << std::endl;
        std::cout << "Paired request/response FIFOs\n" << std::endl;

        for (int i = 0; i < 3; i++) {
            Request req = {i, 0x100 + i * 4, (i % 2 == 0), i * 10};
            std::cout << sc_time_stamp() << " Master: sending req#" << req.id
                      << (req.is_write ? " WRITE" : " READ")
                      << " addr=0x" << std::hex << req.addr << std::dec
                      << std::endl;

            req_out->put(req);

            Response rsp;
            rsp_in->get(rsp);

            std::cout << sc_time_stamp() << " Master: got rsp#" << rsp.id
                      << " data=" << rsp.data
                      << " ok=" << rsp.ok << std::endl;

            wait(5, SC_NS);
        }
    }

    SC_CTOR(Master) { SC_THREAD(run); }
};

SC_MODULE(Slave) {
    sc_port<tlm_blocking_get_if<Request>>  req_in;
    sc_port<tlm_blocking_put_if<Response>> rsp_out;

    void run() {
        while (true) {
            Request req;
            req_in->get(req);

            std::cout << sc_time_stamp() << " Slave: processing req#"
                      << req.id << std::endl;
            wait(10, SC_NS);  // processing time

            Response rsp = {req.id, req.data + 1, true};
            rsp_out->put(rsp);
        }
    }

    SC_CTOR(Slave) { SC_THREAD(run); }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== Demo 16: tlm_fifo, tlm_analysis_port, tlm_req_rsp_channel ==="
              << std::endl;

    // --- Part A: tlm_fifo ---
    std::cout << "\n========== Part A: tlm_fifo ==========" << std::endl;
    std::cout << "Depth=3, fast producer (5ns), slow consumer (15ns)\n" << std::endl;

    tlm_fifo<Packet> fifo("fifo", 3);  // depth 3

    FifoProducer prod("prod");
    prod.out(fifo);          // put interface

    FifoConsumer cons("cons");
    cons.in(fifo);           // get_peek interface

    // tlm_fifo implements BOTH put and get interfaces,
    // so multiple port types can bind to it

    // --- Part B: analysis port ---
    TxSource   src("src");
    Monitor    mon1("mon1", "Monitor-A");
    Monitor    mon2("mon2", "Monitor-B");
    Scoreboard sb("sb");

    // Bind: one port, multiple subscribers
    src.analysis_out.bind(mon1.writer);
    src.analysis_out.bind(mon2.writer);
    src.analysis_out.bind(sb.writer);
    // All three see every write() — fanout is automatic

    // --- Part C: req/rsp channel ---
    tlm_req_rsp_channel<Request, Response> channel("channel");

    Master master("master");
    master.req_out(channel.put_request_export);
    master.rsp_in(channel.get_response_export);

    Slave slave("slave");
    slave.req_in(channel.get_request_export);
    slave.rsp_out(channel.put_response_export);

    sc_start(300, SC_NS);

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "tlm_fifo<T>:" << std::endl;
    std::cout << "  - Typed FIFO with put/get/peek (blocking + non-blocking)" << std::endl;
    std::cout << "  - Like sc_fifo but with TLM interface hierarchy" << std::endl;
    std::cout << "\ntlm_analysis_port<T>:" << std::endl;
    std::cout << "  - 1-to-N broadcast, zero-time, no backpressure" << std::endl;
    std::cout << "  - Subscribers implement tlm_analysis_if<T>::write()" << std::endl;
    std::cout << "  - Perfect for monitors, scoreboards, coverage" << std::endl;
    std::cout << "\ntlm_req_rsp_channel<REQ,RSP>:" << std::endl;
    std::cout << "  - Paired FIFOs for request/response communication" << std::endl;
    std::cout << "  - Master: put_request + get_response" << std::endl;
    std::cout << "  - Slave:  get_request + put_response" << std::endl;

    return 0;
}
