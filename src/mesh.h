#ifndef MESH_H
#define MESH_H

#include <systemc.h>
#include <vector>
#include <sstream>
#include "router.h"
#include "pe.h"

SC_MODULE(Mesh) {
    int mesh_size;

    std::vector<Router*> routers;
    std::vector<PE*>     pes;

    // One fifo per directed link between routers
    std::vector<sc_fifo<Packet>*> h_links_fwd;  // east-bound
    std::vector<sc_fifo<Packet>*> h_links_rev;  // west-bound
    std::vector<sc_fifo<Packet>*> v_links_fwd;  // south-bound
    std::vector<sc_fifo<Packet>*> v_links_rev;  // north-bound

    // PE <-> Router local fifos
    std::vector<sc_fifo<Packet>*> pe_to_r;
    std::vector<sc_fifo<Packet>*> r_to_pe;

    // Stub fifos for unused border ports
    std::vector<sc_fifo<Packet>*> stubs;

    int idx(int x, int y)   { return y * mesh_size + x; }
    int h_idx(int x, int y) { return y * (mesh_size - 1) + x; }
    int v_idx(int x, int y) { return y * mesh_size + x; }

    SC_CTOR(Mesh) : mesh_size(0) {}

    void build(int size) {
        mesh_size = size;
        int total = size * size;

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
                routers[i]->init(x, y, size);

                pes[i] = new PE(pname.str().c_str());
                pes[i]->init(x, y, size);

                pe_to_r[i] = new sc_fifo<Packet>(f1.str().c_str(), BUFFER_DEPTH);
                r_to_pe[i] = new sc_fifo<Packet>(f2.str().c_str(), BUFFER_DEPTH);

                pes[i]->out(    *pe_to_r[i]);
                routers[i]->in[LOCAL](*pe_to_r[i]);

                routers[i]->out[LOCAL](*r_to_pe[i]);
                pes[i]->in(     *r_to_pe[i]);
            }
        }

        int h_count = (size - 1) * size;
        int v_count = size * (size - 1);

        h_links_fwd.resize(h_count);
        h_links_rev.resize(h_count);
        v_links_fwd.resize(v_count);
        v_links_rev.resize(v_count);

        for (int i = 0; i < h_count; i++) {
            h_links_fwd[i] = new sc_fifo<Packet>(BUFFER_DEPTH);
            h_links_rev[i] = new sc_fifo<Packet>(BUFFER_DEPTH);
        }
        for (int i = 0; i < v_count; i++) {
            v_links_fwd[i] = new sc_fifo<Packet>(BUFFER_DEPTH);
            v_links_rev[i] = new sc_fifo<Packet>(BUFFER_DEPTH);
        }

        // Horizontal links
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size - 1; x++) {
                int hi    = h_idx(x, y);
                int left  = idx(x,     y);
                int right = idx(x + 1, y);

                routers[left]->out[EAST]( *h_links_fwd[hi]);
                routers[right]->in[WEST]( *h_links_fwd[hi]);

                routers[right]->out[WEST](*h_links_rev[hi]);
                routers[left]->in[EAST](  *h_links_rev[hi]);
            }
        }

        // Vertical links
        for (int y = 0; y < size - 1; y++) {
            for (int x = 0; x < size; x++) {
                int vi     = v_idx(x, y);
                int top    = idx(x, y);
                int bottom = idx(x, y + 1);

                routers[top]->out[SOUTH](   *v_links_fwd[vi]);
                routers[bottom]->in[NORTH]( *v_links_fwd[vi]);

                routers[bottom]->out[NORTH](*v_links_rev[vi]);
                routers[top]->in[SOUTH](    *v_links_rev[vi]);
            }
        }

        connect_border_stubs(size);
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
        // in-stub: router reads from this but nothing ever writes to it
        sc_fifo<Packet>* sin = new sc_fifo<Packet>(BUFFER_DEPTH);
        stubs.push_back(sin);
        r->in[d](*sin);

        // out-stub: router writes here but nothing reads it — use large depth
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
        for (auto r : routers)     delete r;
        for (auto p : pes)         delete p;
        for (auto f : pe_to_r)     delete f;
        for (auto f : r_to_pe)     delete f;
        for (auto f : h_links_fwd) delete f;
        for (auto f : h_links_rev) delete f;
        for (auto f : v_links_fwd) delete f;
        for (auto f : v_links_rev) delete f;
        for (auto f : stubs)       delete f;
    }
};

#endif
