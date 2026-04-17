// Demo: sc_fifo hop-by-hop  PE -> Router -> Router -> Responder
// =============================================================
// Same topology as demo_lt_hop and demo_at_hop but using sc_fifo<Packet>
// instead of TLM sockets. Each hop is a fifo — blocking write/read
// naturally handles backpressure if a fifo is full.
//
// Two fifo chains:
//   Forward:  PE -[req]-> R1 -[req]-> R2 -[req]-> Responder
//   Backward: PE <-[rsp]- R1 <-[rsp]- R2 <-[rsp]- Responder

#include <systemc.h>
#include <string>

// ---- Simple packet ----
struct Packet {
    enum Cmd { WRITE, READ } cmd;
    uint32_t addr;
    uint32_t data;
    int      id;

    bool operator==(const Packet& o) const {
        return id == o.id && cmd == o.cmd && addr == o.addr && data == o.data;
    }
    friend std::ostream& operator<<(std::ostream& os, const Packet& p) {
        os << "[Pkt#" << p.id
           << (p.cmd == WRITE ? " WRITE" : " READ")
           << " addr=0x" << std::hex << p.addr
           << " data=0x" << p.data << std::dec << "]";
        return os;
    }
};

// ---- Responder ----
SC_MODULE(Responder) {
    sc_fifo_in<Packet>  req;
    sc_fifo_out<Packet> rsp;

    uint32_t mem[256] = {};

    SC_CTOR(Responder) {
        SC_THREAD(run);
    }

    void run() {
        while (true) {
            Packet p = req.read();   // block until request arrives

            std::cout << sc_time_stamp() << " Responder: got " << p << "\n";

            if (p.cmd == Packet::WRITE)
                mem[p.addr] = p.data;
            else
                p.data = mem[p.addr];

            wait(5, SC_NS);          // access latency
            std::cout << sc_time_stamp() << " Responder: sending response\n";
            rsp.write(p);            // block until response fifo has space
        }
    }
};

// ---- Router: one thread for forward, one for backward ----
SC_MODULE(Router) {
    sc_fifo_in<Packet>  req_in;
    sc_fifo_out<Packet> req_out;
    sc_fifo_in<Packet>  rsp_in;
    sc_fifo_out<Packet> rsp_out;

    std::string label;
    sc_time     hop_latency;

    Router(sc_module_name name, const std::string& lbl, sc_time latency)
        : sc_module(name), req_in("req_in"), req_out("req_out"),
          rsp_in("rsp_in"), rsp_out("rsp_out"),
          label(lbl), hop_latency(latency) {
        SC_THREAD(forward);
        SC_THREAD(backward);
    }

    void forward() {
        while (true) {
            Packet p = req_in.read();
            wait(hop_latency);
            std::cout << sc_time_stamp() << " " << label
                      << ": forwarding request " << p << "\n";
            req_out.write(p);
        }
    }

    void backward() {
        while (true) {
            Packet p = rsp_in.read();
            wait(hop_latency);
            std::cout << sc_time_stamp() << " " << label
                      << ": forwarding response " << p << "\n";
            rsp_out.write(p);
        }
    }
};

// ---- PE ----
SC_MODULE(PE) {
    sc_fifo_out<Packet> req;
    sc_fifo_in<Packet>  rsp;

    SC_CTOR(PE) {
        SC_THREAD(run);
    }

    Packet send(Packet::Cmd cmd, uint32_t addr, uint32_t data, int id) {
        Packet p;
        p.cmd  = cmd;
        p.addr = addr;
        p.data = data;
        p.id   = id;

        std::cout << sc_time_stamp() << " PE: sending " << p << "\n";
        req.write(p);           // blocks if router input full

        Packet r = rsp.read();  // blocks until response arrives
        std::cout << sc_time_stamp() << " PE: received response " << r << "\n\n";
        return r;
    }

    void run() {
        std::cout << "--- WRITE ---\n";
        send(Packet::WRITE, 0x10, 0xCAFEBABE, 1);

        std::cout << "--- READ ---\n";
        Packet r = send(Packet::READ, 0x10, 0, 2);
        std::cout << sc_time_stamp() << " PE: read back 0x"
                  << std::hex << r.data << std::dec << "\n";
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "=== sc_fifo hop-by-hop: PE -> R1 -> R2 -> Responder ===\n\n";

    PE        pe("pe");
    Router    r1("r1", "Router1", sc_time(10, SC_NS));
    Router    r2("r2", "Router2", sc_time(10, SC_NS));
    Responder resp("resp");

    // Forward fifos: PE -> R1 -> R2 -> Responder
    sc_fifo<Packet> pe_to_r1(4), r1_to_r2(4), r2_to_resp(4);

    // Backward fifos: Responder -> R2 -> R1 -> PE
    sc_fifo<Packet> resp_to_r2(4), r2_to_r1(4), r1_to_pe(4);

    // Forward chain
    pe.req(pe_to_r1);
    r1.req_in(pe_to_r1);   r1.req_out(r1_to_r2);
    r2.req_in(r1_to_r2);   r2.req_out(r2_to_resp);
    resp.req(r2_to_resp);

    // Backward chain
    resp.rsp(resp_to_r2);
    r2.rsp_in(resp_to_r2); r2.rsp_out(r2_to_r1);
    r1.rsp_in(r2_to_r1);   r1.rsp_out(r1_to_pe);
    pe.rsp(r1_to_pe);

    sc_start();
    return 0;
}
