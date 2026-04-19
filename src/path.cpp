//An extremely basic path

static const int DESTINATIONS = 3; // Number of possible destinations

#include <systemc.h>

//define a packet class
class Packet {
public:
    int src_id; // Source coordinates
    int dst; // Destination direction
    int payload;      // Payload data
    Packet(int src_id = 0, int dst, int p = 0)
        : src_id(src_id), dst(dst), payload(p) {}
    int get_src() { 
        return src_id; 
    }
    int get_dst() { 
        return dst; 
    }
    int get_payload() { 
        return payload; 
    }
    void set_payload(int p) { 
        payload = p; 
    }
};

//Define a generator module that creates packets
SC_MODULE(PacketGenerator) {
    sc_fifo_out<Packet> out; // Output port for packets

    int position; // Position of the generator in the mesh

    void generate() {
        for (int i = 1; i <= DESTINATIONS; i++) {
            Packet p(0, i, i*10); 
            std::cout << sc_time_stamp() << " Generator: created packet with payload " << p.get_payload() << std::endl;
            out.write(p); // Write packet to output FIFO
            wait(10, SC_NS); // Wait for 10 ns before generating next packet
        }
    }

    PacketGenerator(sc_module_name name, int position) 
    : sc_module(name), position(position) {
        SC_THREAD(generate); // Register the generate thread
    }
};

SC_MODULE(PacketRouter) {
    sc_fifo_in<Packet> in; // Input port for packets
    sc_fifo_out<Packet> out[DESTINATIONS]; // Output port for packets

    void route() {
        while (true) {
            Packet p = in.read(); // Read packet from input FIFO
            std::cout << sc_time_stamp() << " Router: received packet with payload " << p.get_payload() << std::endl;
            int dst = p.get_dst(); // Determine output port based on destination
            out[dst].write(p); // Write packet to output FIFO
            wait(5, SC_NS); // Wait for 5 ns to simulate routing delay
        }
    }

    PacketRouter(sc_module_name name) : sc_module(name) {
        SC_THREAD(route); // Register the route thread
    }
};

int sc_main(int argc, char* argv[]) {
    PacketGenerator gen("gen", 0); // Instantiate the packet generator
    sc_start(); // Start the simulation
    return 0;
}




