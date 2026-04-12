#include <iostream>

class Signal{
public:
    std::string name;
    int         value;

    //Constructor: runs when an object is created
    Signal(std::string n, int w)
        : name(n), value(0)     // initializer list
    {
        std::cout << "Signal '" << name << "' created" << "\n";
    }

    void write(int v) {
        value = v;
        std::cout << name << " <= " << value << "\n";
    }

    int read() const {
        return value;
    }   
};

int main() {
    Signal clk("a", 1);
    Signal bus("b", 1);

    clk.write(1);
    bus.write(0xDEAD);

    std::cout << "clk = " << clk.read() << "\n";
    std::cout << "bus = " << bus.read() << "\n";
}