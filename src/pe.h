#ifndef PE_H
#define PE_H

#include <systemc.h>
#include <cstdlib>
#include "packet.h"
#include "txlog.h"

SC_MODULE(PE) {
    int pos_x;
    int pos_y;
    int mesh_size;
    int pkt_count;

    sc_fifo_out<Packet> out;
    sc_fifo_in<Packet>  in;

    int packets_sent;
    int packets_received;

    SC_CTOR(PE)
        : pos_x(0), pos_y(0), mesh_size(0), pkt_count(0),
          packets_sent(0), packets_received(0) {
        SC_THREAD(generate);
        SC_THREAD(consume);
    }

    void init(int x, int y, int size) {
        pos_x = x;
        pos_y = y;
        mesh_size = size;
    }

    void generate() {
        while (true) {
            wait(10 + rand() % 40, SC_NS);

            int dst_x, dst_y;
            do {
                dst_x = rand() % mesh_size;
                dst_y = rand() % mesh_size;
            } while (dst_x == pos_x && dst_y == pos_y);

            Packet pkt(pos_x, pos_y, dst_x, dst_y, pkt_count++, rand() % 256);
            TxLog::instance().log_send(pos_x, pos_y, pkt);
            out.write(pkt);  // blocks if router input full
            packets_sent++;
        }
    }

    void consume() {
        while (true) {
            Packet pkt = in.read();  // blocks until packet arrives
            packets_received++;
            TxLog::instance().log_recv(pos_x, pos_y, pkt);
        }
    }
};

#endif
