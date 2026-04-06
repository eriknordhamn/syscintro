#include <systemc.h>

SC_MODULE(counter) {
    sc_in<bool>  clk;
    sc_out<int>  count;
    int n = 0;

    void tick() { n++; 
        cout<< "Tick\n";
        count.write(n); }

    SC_CTOR(counter) {
        SC_METHOD(tick);
        sensitive << clk.pos();
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock       clk("clk", 10, SC_NS);
    sc_signal<int> c;
    counter u("u");
    u.clk(clk); u.count(c);
    sc_start(100, SC_NS);
    return 0;
}
