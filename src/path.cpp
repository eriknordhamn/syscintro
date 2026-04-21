//An extremely basic path
#include <systemc.h>

static const int DESTINATIONS = 4; // Number of possible destinations


//define a packet class
class Packet {
public:
    int src_id; // Source ID
    int dst_id; // Destination ID
    int payload;      // Payload data
    Packet(int src_id = 0, int dst_id = 0, int payload = 0)
        : src_id(src_id), dst_id(dst_id), payload(payload) {}
    int get_src() { 
        return src_id; 
    }
    int get_dst() { 
        return dst_id; 
    }
    int get_payload() { 
        return payload; 
    }
    void set_payload(int p) { payload = p; }

    friend std::ostream& operator<<(std::ostream& os, const Packet& p) {
        return os << "[src=" << p.src_id << " dst=" << p.dst_id
                  << " payload=" << p.payload << "]";
    }
};

//Define a generator module that creates packets
SC_MODULE(PacketGenerator) {
    sc_fifo_out<Packet> out; // Output port for packets

    int position; // Position of the generator in the mesh

    void generate() {
        for (int i = 0; i < DESTINATIONS; i++) {
            Packet p(0, i, i*10); 
            std::cout << sc_time_stamp() << " Generator: created packet with payload " << p.get_payload() 
            << "to destination " << p.get_dst() << std::endl;
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

SC_MODULE(PacketDestination) {
    sc_fifo_in<Packet> in; // Input port for packets

    int dst_id;

    void respond() {
        while (true) {
            Packet p = in.read(); // Read packet from input FIFO
            std::cout << sc_time_stamp() << " Responder: received packet with payload " << p.get_payload() 
            << "at destination " << p.get_dst() << std::endl;
        }
    }

    PacketDestination(sc_module_name name, int dst_id) 
    : sc_module(name), dst_id(dst_id) {
        SC_THREAD(respond); // Register the respond thread
    }
};

int sc_main(int argc, char* argv[]) {

    sc_fifo<Packet> src_router("src_out"); // FIFO between generator and router
    sc_fifo<Packet> router_dst0("router_out0"); // FIFO between router and destinations
    sc_fifo<Packet> router_dst1("router_out1"); // FIFO between router and destinations
    sc_fifo<Packet> router_dst2("router_out2"); // FIFO between router and destinations
    sc_fifo<Packet> router_dst3("router_out3"); // FIFO between router and destinations 
    
    PacketGenerator gen("gen", 0);
    PacketRouter router("router");
    PacketDestination dest0("dst0", 0);
    PacketDestination dest1("dst1", 1);
    PacketDestination dest2("dst2", 2);
    PacketDestination dest3("dst3", 3);

    gen.out(src_router);
    router.in(src_router);
    router.out[0](router_dst0);
    router.out[1](router_dst1);
    router.out[2](router_dst2);
    router.out[3](router_dst3);
    dest0.in(router_dst0);
    dest1.in(router_dst1);
    dest2.in(router_dst2);
    dest3.in(router_dst3);


    sc_start(); // Start the simulation
    return 0;
}




