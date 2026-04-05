#ifndef ROUTER_H
#define ROUTER_H

#include <systemc.h>
#include <queue>
#include "packet.h"

// Buffer depth for each input port
static const int BUFFER_DEPTH = 4;

SC_MODULE(Router) {
    // Position in the mesh
    int pos_x;
    int pos_y;
    int mesh_size;

    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> rst;

    // Input ports (valid + data) for each direction
    sc_in<bool>   in_valid[NUM_DIRS];
    sc_in<Packet> in_data[NUM_DIRS];
    sc_out<bool>  in_ready[NUM_DIRS];

    // Output ports (valid + data) for each direction
    sc_out<bool>   out_valid[NUM_DIRS];
    sc_out<Packet> out_data[NUM_DIRS];
    sc_in<bool>    out_ready[NUM_DIRS];

    // Internal input buffers
    std::queue<Packet> input_buffer[NUM_DIRS];

    SC_CTOR(Router)
        : pos_x(0), pos_y(0), mesh_size(0) {
        SC_METHOD(route);
        sensitive << clk.pos();
        dont_initialize();
    }

    void init(int x, int y, int size) {
        pos_x = x;
        pos_y = y;
        mesh_size = size;
    }

    // XY routing: route in X direction first, then Y
    Direction compute_route(const Packet& pkt) {
        if (pkt.dst_x < pos_x) return WEST;
        if (pkt.dst_x > pos_x) return EAST;
        if (pkt.dst_y < pos_y) return NORTH;
        if (pkt.dst_y > pos_y) return SOUTH;
        return LOCAL;
    }

    void route() {
        if (rst.read()) {
            for (int i = 0; i < NUM_DIRS; i++) {
                out_valid[i].write(false);
                in_ready[i].write(true);
                while (!input_buffer[i].empty())
                    input_buffer[i].pop();
            }
            return;
        }

        // Read incoming flits into buffers
        for (int i = 0; i < NUM_DIRS; i++) {
            if (in_valid[i].read() &&
                (int)input_buffer[i].size() < BUFFER_DEPTH) {
                input_buffer[i].push(in_data[i].read());
            }
            in_ready[i].write((int)input_buffer[i].size() < BUFFER_DEPTH);
        }

        // Track which output ports are claimed this cycle
        bool out_used[NUM_DIRS] = {false};

        // Clear all outputs first
        for (int i = 0; i < NUM_DIRS; i++) {
            out_valid[i].write(false);
        }

        // Round-robin arbitration starting from LOCAL
        for (int i = 0; i < NUM_DIRS; i++) {
            if (input_buffer[i].empty())
                continue;

            Packet pkt = input_buffer[i].front();
            Direction out_dir = compute_route(pkt);

            if (!out_used[out_dir] && out_ready[out_dir].read()) {
                out_data[out_dir].write(pkt);
                out_valid[out_dir].write(true);
                out_used[out_dir] = true;
                input_buffer[i].pop();
            }
        }
    }
};

#endif
