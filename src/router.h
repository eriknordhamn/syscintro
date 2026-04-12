#ifndef ROUTER_H
#define ROUTER_H

#include <systemc.h>
#include "packet.h"
#include "txlog.h"

static const int BUFFER_DEPTH = 4;

SC_MODULE(Router) {
    int pos_x;
    int pos_y;
    int mesh_size;
    sc_time routing_latency;

    sc_fifo_in<Packet>  in[NUM_DIRS];
    sc_fifo_out<Packet> out[NUM_DIRS];

    SC_CTOR(Router) : pos_x(0), pos_y(0), mesh_size(0), routing_latency(SC_ZERO_TIME) {
        SC_THREAD(route);
    }

    void init(int x, int y, int size, sc_time latency = SC_ZERO_TIME) {
        pos_x = x;
        pos_y = y;
        mesh_size = size;
        routing_latency = latency;
    }

    Direction compute_route(const Packet& pkt) {
        if (pkt.dst_x < pos_x) return WEST;
        if (pkt.dst_x > pos_x) return EAST;
        if (pkt.dst_y < pos_y) return NORTH;
        if (pkt.dst_y > pos_y) return SOUTH;
        return LOCAL;
    }

    void route() {
        while (true) {
            bool processed = false;
            for (int i = 0; i < NUM_DIRS; i++) {
                if (in[i].num_available() > 0) {
                    Packet pkt = in[i].read();
                    if (routing_latency != SC_ZERO_TIME)
                        wait(routing_latency);
                    Direction d = compute_route(pkt);
                    TxLog::instance().log_forward(pos_x, pos_y, pkt, (Direction)i, d);
                    out[d].write(pkt);
                    processed = true;
                }
            }
            if (!processed) {
                wait(in[LOCAL].data_written_event() |
                     in[NORTH].data_written_event() |
                     in[SOUTH].data_written_event() |
                     in[EAST].data_written_event()  |
                     in[WEST].data_written_event());
            }
        }
    }
};

#endif
