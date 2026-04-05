#ifndef TXLOG_H
#define TXLOG_H

#include <fstream>
#include <string>
#include <systemc.h>
#include "packet.h"

// Singleton transaction logger - writes CSV for postprocessing
class TxLog {
public:
    static TxLog& instance() {
        static TxLog log;
        return log;
    }

    void open(const std::string& filename) {
        file.open(filename);
        file << "time_ns,event,node_x,node_y,pkt_id,src_x,src_y,"
             << "dst_x,dst_y,in_port,out_port,payload\n";
    }

    void close() {
        if (file.is_open()) file.close();
    }

    // PE sends a packet into the network
    void log_send(int node_x, int node_y, const Packet& pkt) {
        write_row("SEND", node_x, node_y, pkt, "-", "LOCAL");
    }

    // PE receives a packet from the network
    void log_recv(int node_x, int node_y, const Packet& pkt) {
        write_row("RECV", node_x, node_y, pkt, "LOCAL", "-");
    }

    // Router forwards a packet from in_port to out_port
    void log_forward(int node_x, int node_y, const Packet& pkt,
                     Direction in_dir, Direction out_dir) {
        write_row("FWD", node_x, node_y, pkt,
                  dir_to_string(in_dir), dir_to_string(out_dir));
    }

    // Router buffers an incoming packet
    void log_buffer(int node_x, int node_y, const Packet& pkt,
                    Direction in_dir) {
        write_row("BUF", node_x, node_y, pkt,
                  dir_to_string(in_dir), "-");
    }

    // Router drops a packet (buffer full)
    void log_drop(int node_x, int node_y, const Packet& pkt,
                  Direction in_dir) {
        write_row("DROP", node_x, node_y, pkt,
                  dir_to_string(in_dir), "-");
    }

private:
    std::ofstream file;

    TxLog() {}
    TxLog(const TxLog&);
    TxLog& operator=(const TxLog&);

    void write_row(const char* event, int nx, int ny, const Packet& pkt,
                   const char* in_port, const char* out_port) {
        if (!file.is_open()) return;
        file << sc_time_stamp().to_double() / 1000.0 << ","  // ns
             << event << ","
             << nx << "," << ny << ","
             << pkt.id << ","
             << pkt.src_x << "," << pkt.src_y << ","
             << pkt.dst_x << "," << pkt.dst_y << ","
             << in_port << "," << out_port << ","
             << pkt.payload << "\n";
    }
};

#endif
