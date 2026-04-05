#ifndef MESH_H
#define MESH_H

#include <systemc.h>
#include <vector>
#include <sstream>
#include "router.h"
#include "pe.h"

SC_MODULE(Mesh) {
    int mesh_size;

    sc_in<bool> clk;
    sc_in<bool> rst;

    // Flattened arrays: index = y * mesh_size + x
    std::vector<Router*> routers;
    std::vector<PE*>     pes;

    // Signals between routers (horizontal and vertical links)
    // Horizontal: router(x,y) EAST <-> router(x+1,y) WEST
    // Vertical:   router(x,y) SOUTH <-> router(x,y+1) NORTH

    // Inter-router link signals
    struct Link {
        sc_signal<bool>   valid;
        sc_signal<Packet> data;
        sc_signal<bool>   ready;
    };

    std::vector<Link*> h_links_fwd;  // east-bound: (x,y) -> (x+1,y)
    std::vector<Link*> h_links_rev;  // west-bound: (x+1,y) -> (x,y)
    std::vector<Link*> v_links_fwd;  // south-bound: (x,y) -> (x,y+1)
    std::vector<Link*> v_links_rev;  // north-bound: (x,y+1) -> (x,y)

    // PE <-> Router local port signals
    struct LocalLink {
        sc_signal<bool>   pe_to_r_valid;
        sc_signal<Packet> pe_to_r_data;
        sc_signal<bool>   pe_to_r_ready;
        sc_signal<bool>   r_to_pe_valid;
        sc_signal<Packet> r_to_pe_data;
        sc_signal<bool>   r_to_pe_ready;
    };

    std::vector<LocalLink*> local_links;

    // Stub signals for unused border ports
    struct StubSignals {
        sc_signal<bool>   valid;
        sc_signal<Packet> data;
        sc_signal<bool>   ready;
    };

    std::vector<StubSignals*> stubs;

    int idx(int x, int y) { return y * mesh_size + x; }

    // Horizontal link index: (mesh_size-1) links per row
    int h_idx(int x, int y) { return y * (mesh_size - 1) + x; }

    // Vertical link index: mesh_size links per row
    int v_idx(int x, int y) { return y * mesh_size + x; }

    SC_CTOR(Mesh) : mesh_size(0) {}

    void build(int size) {
        mesh_size = size;
        int total = size * size;

        // Allocate routers and PEs
        routers.resize(total);
        pes.resize(total);
        local_links.resize(total);

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int i = idx(x, y);

                std::ostringstream rname, pname;
                rname << "router_" << x << "_" << y;
                pname << "pe_" << x << "_" << y;

                routers[i] = new Router(rname.str().c_str());
                routers[i]->init(x, y, size);
                routers[i]->clk(clk);
                routers[i]->rst(rst);

                pes[i] = new PE(pname.str().c_str());
                pes[i]->init(x, y, size);
                pes[i]->clk(clk);
                pes[i]->rst(rst);

                local_links[i] = new LocalLink();
            }
        }

        // Allocate inter-router links
        int h_count = (size - 1) * size;  // horizontal links
        int v_count = size * (size - 1);  // vertical links

        h_links_fwd.resize(h_count);
        h_links_rev.resize(h_count);
        v_links_fwd.resize(v_count);
        v_links_rev.resize(v_count);

        for (int i = 0; i < h_count; i++) {
            h_links_fwd[i] = new Link();
            h_links_rev[i] = new Link();
        }
        for (int i = 0; i < v_count; i++) {
            v_links_fwd[i] = new Link();
            v_links_rev[i] = new Link();
        }

        // Connect local ports (PE <-> Router)
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int i = idx(x, y);
                LocalLink* ll = local_links[i];

                // PE output -> Router LOCAL input
                pes[i]->out_valid(ll->pe_to_r_valid);
                pes[i]->out_data(ll->pe_to_r_data);
                pes[i]->out_ready(ll->pe_to_r_ready);

                routers[i]->in_valid[LOCAL](ll->pe_to_r_valid);
                routers[i]->in_data[LOCAL](ll->pe_to_r_data);
                routers[i]->in_ready[LOCAL](ll->pe_to_r_ready);

                // Router LOCAL output -> PE input
                routers[i]->out_valid[LOCAL](ll->r_to_pe_valid);
                routers[i]->out_data[LOCAL](ll->r_to_pe_data);
                routers[i]->out_ready[LOCAL](ll->r_to_pe_ready);

                pes[i]->in_valid(ll->r_to_pe_valid);
                pes[i]->in_data(ll->r_to_pe_data);
                pes[i]->in_ready(ll->r_to_pe_ready);
            }
        }

        // Connect horizontal links (EAST/WEST)
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size - 1; x++) {
                int hi = h_idx(x, y);
                int left = idx(x, y);
                int right = idx(x + 1, y);

                Link* fwd = h_links_fwd[hi];  // left EAST -> right WEST input
                Link* rev = h_links_rev[hi];   // right WEST -> left EAST input

                // Left router EAST output -> fwd signals -> Right router WEST input
                routers[left]->out_valid[EAST](fwd->valid);
                routers[left]->out_data[EAST](fwd->data);
                routers[left]->out_ready[EAST](fwd->ready);

                routers[right]->in_valid[WEST](fwd->valid);
                routers[right]->in_data[WEST](fwd->data);
                routers[right]->in_ready[WEST](fwd->ready);

                // Right router WEST output -> rev signals -> Left router EAST input
                routers[right]->out_valid[WEST](rev->valid);
                routers[right]->out_data[WEST](rev->data);
                routers[right]->out_ready[WEST](rev->ready);

                routers[left]->in_valid[EAST](rev->valid);
                routers[left]->in_data[EAST](rev->data);
                routers[left]->in_ready[EAST](rev->ready);
            }
        }

        // Connect vertical links (SOUTH/NORTH)
        for (int y = 0; y < size - 1; y++) {
            for (int x = 0; x < size; x++) {
                int vi = v_idx(x, y);
                int top = idx(x, y);
                int bottom = idx(x, y + 1);

                Link* fwd = v_links_fwd[vi];  // top SOUTH -> bottom NORTH input
                Link* rev = v_links_rev[vi];   // bottom NORTH -> top SOUTH input

                // Top router SOUTH output -> fwd -> Bottom router NORTH input
                routers[top]->out_valid[SOUTH](fwd->valid);
                routers[top]->out_data[SOUTH](fwd->data);
                routers[top]->out_ready[SOUTH](fwd->ready);

                routers[bottom]->in_valid[NORTH](fwd->valid);
                routers[bottom]->in_data[NORTH](fwd->data);
                routers[bottom]->in_ready[NORTH](fwd->ready);

                // Bottom router NORTH output -> rev -> Top router SOUTH input
                routers[bottom]->out_valid[NORTH](rev->valid);
                routers[bottom]->out_data[NORTH](rev->data);
                routers[bottom]->out_ready[NORTH](rev->ready);

                routers[top]->in_valid[SOUTH](rev->valid);
                routers[top]->in_data[SOUTH](rev->data);
                routers[top]->in_ready[SOUTH](rev->ready);
            }
        }

        // Stub unused border ports
        connect_border_stubs(size);
    }

    void connect_border_stubs(int size) {
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int i = idx(x, y);

                // North border (y == 0): NORTH port is unused
                if (y == 0) stub_input(routers[i], NORTH);
                // South border (y == size-1): SOUTH port is unused
                if (y == size - 1) stub_input(routers[i], SOUTH);
                // West border (x == 0): WEST port is unused
                if (x == 0) stub_input(routers[i], WEST);
                // East border (x == size-1): EAST port is unused
                if (x == size - 1) stub_input(routers[i], EAST);
            }
        }
    }

    void stub_input(Router* r, Direction d) {
        StubSignals* s = new StubSignals();
        stubs.push_back(s);

        // Stub the input side (no one sends to this port)
        r->in_valid[d](s->valid);
        r->in_data[d](s->data);
        r->in_ready[d](s->ready);

        // Stub the output side (no one receives from this port)
        StubSignals* s2 = new StubSignals();
        stubs.push_back(s2);
        r->out_valid[d](s2->valid);
        r->out_data[d](s2->data);
        r->out_ready[d](s2->ready);
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
        for (auto r : routers) delete r;
        for (auto p : pes) delete p;
        for (auto l : local_links) delete l;
        for (auto l : h_links_fwd) delete l;
        for (auto l : h_links_rev) delete l;
        for (auto l : v_links_fwd) delete l;
        for (auto l : v_links_rev) delete l;
        for (auto s : stubs) delete s;
    }
};

#endif
