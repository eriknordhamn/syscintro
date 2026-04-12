#ifndef PIPE_LINK_H
#define PIPE_LINK_H

#include <systemc.h>
#include <vector>
#include <sstream>
#include "packet.h"

// PipeLink — a pipelined channel between two routers.
//
// Each stage introduces exactly one clock_period of latency, allowing
// multiple packets to be in-flight simultaneously (true pipeline).
// extra_buffer adds depth to each internal stage FIFO, decoupling
// adjacent stages so a stalled downstream stage doesn't immediately
// back-pressure the upstream one.
//
// Wiring in the mesh:
//   Router_src::out[dir] --> [input sc_fifo] --> PipeLink::in
//   PipeLink::out --> [output sc_fifo] --> Router_dst::in[dir]

SC_MODULE(PipeLink) {
    sc_fifo_in<Packet>  in;
    sc_fifo_out<Packet> out;

    const int     num_stages;
    const int     extra_buffer;  // extra depth per internal stage FIFO (min 1 per stage)
    const sc_time clock_period;

    // Internal stage FIFOs: num_stages - 1 entries (first stage reads from port,
    // last stage writes to port; intermediate stages talk to each other).
    std::vector<sc_fifo<Packet>*> stage_fifos;

    PipeLink(sc_module_name name, int stages, int extra_buf, sc_time period)
        : sc_module(name),
          num_stages(stages),
          extra_buffer(extra_buf),
          clock_period(period) {}

    // Create internal FIFOs and spawn one thread per stage.
    // Called by the SystemC kernel at the end of elaboration, after all
    // port bindings in Mesh::build() have completed.
    void before_end_of_elaboration() override {
        int depth = 1 + extra_buffer;
        for (int i = 0; i < num_stages - 1; i++) {
            std::ostringstream n;
            n << name() << "_sf" << i;
            stage_fifos.push_back(new sc_fifo<Packet>(n.str().c_str(), depth));
        }
        for (int i = 0; i < num_stages; i++) {
            sc_spawn([this, i]() { stage_proc(i); },
                     sc_gen_unique_name("stage"));
        }
    }

    // One thread per stage: read -> wait one cycle -> write to next stage.
    void stage_proc(int idx) {
        while (true) {
            Packet pkt = (idx == 0) ? in.read()
                                    : stage_fifos[idx - 1]->read();
            wait(clock_period);
            if (idx == num_stages - 1)
                out.write(pkt);
            else
                stage_fifos[idx]->write(pkt);
        }
    }

    ~PipeLink() {
        for (auto* f : stage_fifos) delete f;
    }
};

#endif
