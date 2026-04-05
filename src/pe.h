#ifndef PE_H
#define PE_H

#include <systemc.h>
#include <cstdlib>
#include "packet.h"

// Processing Element: generates and consumes packets
SC_MODULE(PE) {
    int pos_x;
    int pos_y;
    int mesh_size;
    int pkt_count;

    sc_in<bool> clk;
    sc_in<bool> rst;

    // Interface to local router port
    sc_out<bool>   out_valid;
    sc_out<Packet> out_data;
    sc_in<bool>    out_ready;

    sc_in<bool>    in_valid;
    sc_in<Packet>  in_data;
    sc_out<bool>   in_ready;

    // Statistics
    int packets_sent;
    int packets_received;

    SC_CTOR(PE)
        : pos_x(0), pos_y(0), mesh_size(0), pkt_count(0),
          packets_sent(0), packets_received(0) {
        SC_METHOD(generate);
        sensitive << clk.pos();
        dont_initialize();

        SC_METHOD(consume);
        sensitive << clk.pos();
        dont_initialize();
    }

    void init(int x, int y, int size) {
        pos_x = x;
        pos_y = y;
        mesh_size = size;
    }

    void generate() {
        if (rst.read()) {
            out_valid.write(false);
            packets_sent = 0;
            pkt_count = 0;
            return;
        }

        out_valid.write(false);

        // Generate a packet with ~20% probability
        if (out_ready.read() && (rand() % 5 == 0)) {
            int dst_x, dst_y;
            do {
                dst_x = rand() % mesh_size;
                dst_y = rand() % mesh_size;
            } while (dst_x == pos_x && dst_y == pos_y);

            Packet pkt(pos_x, pos_y, dst_x, dst_y, pkt_count++,
                        rand() % 256);

            out_data.write(pkt);
            out_valid.write(true);
            packets_sent++;

            std::cout << sc_time_stamp() << " PE(" << pos_x << ","
                      << pos_y << ") SENT " << pkt << std::endl;
        }
    }

    void consume() {
        in_ready.write(true);

        if (rst.read()) {
            packets_received = 0;
            return;
        }

        if (in_valid.read()) {
            Packet pkt = in_data.read();
            packets_received++;

            std::cout << sc_time_stamp() << " PE(" << pos_x << ","
                      << pos_y << ") RECV " << pkt << std::endl;
        }
    }
};

#endif
