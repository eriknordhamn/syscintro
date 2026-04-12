#ifndef MESH_H
#define MESH_H

#include <systemc.h>
#include <vector>
#include <sstream>
#include "router.h"
#include "pe.h"
#include "pipe_link.h"

SC_MODULE(Mesh) {
    int mesh_size;

    std::vector<Router*>          routers;
    std::vector<PE*>              pes;

    // PE <-> Router local fifos
    std::vector<sc_fifo<Packet>*> pe_to_r;
    std::vector<sc_fifo<Packet>*> r_to_pe;

    // All inter-router link fifos (input + output side of each PipeLink)
    std::vector<sc_fifo<Packet>*> link_fifos;

    // All PipeLink modules (one per directed inter-router link)
    std::vector<PipeLink*> pipes;

    // Stub fifos for unused border ports
    std::vector<sc_fifo<Packet>*> stubs;

    int idx(int x, int y)   { return y * mesh_size + x; }
    int h_idx(int x, int y) { return y * (mesh_size - 1) + x; }
    int v_idx(int x, int y) { return y * mesh_size + x; }

    SC_CTOR(Mesh) : mesh_size(0) {}

    void build(int size,
               int latency_cycles    = 1,
               sc_time clock_period  = sc_time(1, SC_NS),
               int pipe_stages       = 1,
               int pipe_extra_buffer = 0) {
        mesh_size = size;
        int total = size * size;
        sc_time routing_latency = latency_cycles * clock_period;

        routers.resize(total);
        pes.resize(total);
        pe_to_r.resize(total);
        r_to_pe.resize(total);

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int i = idx(x, y);

                std::ostringstream rname, pname, f1, f2;
                rname << "router_" << x << "_" << y;
                pname << "pe_"     << x << "_" << y;
                f1    << "pe_to_r_" << x << "_" << y;
                f2    << "r_to_pe_" << x << "_" << y;

                routers[i] = new Router(rname.str().c_str());
                routers[i]->init(x, y, size, routing_latency);

                pes[i] = new PE(pname.str().c_str());
                pes[i]->init(x, y, size);

                pe_to_r[i] = new sc_fifo<Packet>(f1.str().c_str(), BUFFER_DEPTH);
                r_to_pe[i] = new sc_fifo<Packet>(f2.str().c_str(), BUFFER_DEPTH);

                pes[i]->out(          *pe_to_r[i]);
                routers[i]->in[LOCAL](*pe_to_r[i]);

                routers[i]->out[LOCAL](*r_to_pe[i]);
                pes[i]->in(            *r_to_pe[i]);
            }
        }

        // Horizontal links
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size - 1; x++) {
                int left  = idx(x,     y);
                int right = idx(x + 1, y);

                std::ostringstream hf, hr;
                hf << "hf_" << x << "_" << y;
                hr << "hr_" << x << "_" << y;

                make_link(routers[left]->out[EAST],  routers[right]->in[WEST],
                          pipe_stages, pipe_extra_buffer, clock_period, hf.str());
                make_link(routers[right]->out[WEST], routers[left]->in[EAST],
                          pipe_stages, pipe_extra_buffer, clock_period, hr.str());
            }
        }

        // Vertical links
        for (int y = 0; y < size - 1; y++) {
            for (int x = 0; x < size; x++) {
                int top    = idx(x, y);
                int bottom = idx(x, y + 1);

                std::ostringstream vf, vr;
                vf << "vf_" << x << "_" << y;
                vr << "vr_" << x << "_" << y;

                make_link(routers[top]->out[SOUTH],    routers[bottom]->in[NORTH],
                          pipe_stages, pipe_extra_buffer, clock_period, vf.str());
                make_link(routers[bottom]->out[NORTH], routers[top]->in[SOUTH],
                          pipe_stages, pipe_extra_buffer, clock_period, vr.str());
            }
        }

        connect_border_stubs(size);
    }

    // Create a PipeLink and the two endpoint FIFOs connecting src_out to dst_in.
    void make_link(sc_fifo_out<Packet>& src_out, sc_fifo_in<Packet>& dst_in,
                   int stages, int extra_buf, sc_time period,
                   const std::string& base_name) {
        auto* in_f  = new sc_fifo<Packet>((base_name + "_i").c_str(), BUFFER_DEPTH);
        auto* out_f = new sc_fifo<Packet>((base_name + "_o").c_str(), BUFFER_DEPTH);
        auto* pipe  = new PipeLink((base_name + "_p").c_str(), stages, extra_buf, period);

        src_out(*in_f);
        pipe->in(*in_f);
        pipe->out(*out_f);
        dst_in(*out_f);

        link_fifos.push_back(in_f);
        link_fifos.push_back(out_f);
        pipes.push_back(pipe);
    }

    void connect_border_stubs(int size) {
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int i = idx(x, y);
                if (y == 0)        stub_port(routers[i], NORTH);
                if (y == size - 1) stub_port(routers[i], SOUTH);
                if (x == 0)        stub_port(routers[i], WEST);
                if (x == size - 1) stub_port(routers[i], EAST);
            }
        }
    }

    void stub_port(Router* r, Direction d) {
        sc_fifo<Packet>* sin = new sc_fifo<Packet>(BUFFER_DEPTH);
        stubs.push_back(sin);
        r->in[d](*sin);

        sc_fifo<Packet>* sout = new sc_fifo<Packet>(1024);
        stubs.push_back(sout);
        r->out[d](*sout);
    }

    void print_stats() {
        int total_sent = 0, total_recv = 0;
        std::cout << "\n===== Mesh Network Statistics =====" << std::endl;
        for (int y = 0; y < mesh_size; y++) {
            for (int x = 0; x < mesh_size; x++) {
                int i = idx(x, y);
                std::cout << "PE(" << x << "," << y << "): sent="
                          << pes[i]->packets_sent << " recv="
                          << pes[i]->packets_received << std::endl;
                total_sent += pes[i]->packets_sent;
                total_recv += pes[i]->packets_received;
            }
        }
        std::cout << "Total: sent=" << total_sent
                  << " recv=" << total_recv << std::endl;
        std::cout << "===================================" << std::endl;
    }

    ~Mesh() {
        for (auto* r : routers)    delete r;
        for (auto* p : pes)        delete p;
        for (auto* f : pe_to_r)    delete f;
        for (auto* f : r_to_pe)    delete f;
        for (auto* f : link_fifos) delete f;
        for (auto* p : pipes)      delete p;
        for (auto* f : stubs)      delete f;
    }
};

#endif
