#ifndef PACKET_H
#define PACKET_H

#include <systemc.h>
#include <iostream>

struct Packet {
    int src_x;
    int src_y;
    int dst_x;
    int dst_y;
    int id;
    int payload;

    Packet()
        : src_x(0), src_y(0), dst_x(0), dst_y(0), id(0), payload(0) {}

    Packet(int sx, int sy, int dx, int dy, int pid, int data)
        : src_x(sx), src_y(sy), dst_x(dx), dst_y(dy), id(pid), payload(data) {}

    bool operator==(const Packet& other) const {
        return src_x == other.src_x && src_y == other.src_y &&
               dst_x == other.dst_x && dst_y == other.dst_y &&
               id == other.id && payload == other.payload;
    }

    friend std::ostream& operator<<(std::ostream& os, const Packet& p) {
        os << "[Pkt#" << p.id
           << " (" << p.src_x << "," << p.src_y << ")"
           << "->(" << p.dst_x << "," << p.dst_y << ")"
           << " data=" << p.payload << "]";
        return os;
    }

    friend void sc_trace(sc_trace_file* tf, const Packet& p,
                         const std::string& name) {
        sc_trace(tf, p.id, name + ".id");
        sc_trace(tf, p.src_x, name + ".src_x");
        sc_trace(tf, p.src_y, name + ".src_y");
        sc_trace(tf, p.dst_x, name + ".dst_x");
        sc_trace(tf, p.dst_y, name + ".dst_y");
        sc_trace(tf, p.payload, name + ".payload");
    }
};

enum Direction { LOCAL = 0, NORTH = 1, SOUTH = 2, EAST = 3, WEST = 4, NUM_DIRS = 5 };

inline const char* dir_to_string(Direction d) {
    static const char* names[] = {"LOCAL", "NORTH", "SOUTH", "EAST", "WEST"};
    return names[d];
}

#endif
