// C++ Primer 1: Classes, constructors, member initializer lists
// =============================================================
// Concepts:
//   - Class vs struct (same thing, different default access)
//   - Member variables and member functions
//   - Constructor and member initializer list  ": member(value)"
//   - Why the initializer list matters (runs before the body)
//
// SystemC connection:
//   SC_MODULE(Foo) expands to: struct Foo : sc_module { ... };
//   SC_CTOR(Foo) expands to:   Foo(sc_module_name name) : sc_module(name)
//   socket("socket") in SC_CTOR is a member initializer — same pattern here.

#include <iostream>
#include <string>

// A class is a blueprint. An object is an instance of that blueprint.
class Signal {
public:
    // Member variables — each object gets its own copy
    std::string name;
    int         value;
    int         width;

    // Constructor: called when an object is created
    // The ": name(n), value(0), width(w)" part is the MEMBER INITIALIZER LIST.
    // It runs BEFORE the constructor body and is the correct place to
    // initialize members — especially objects that have their own constructors.
    Signal(std::string n, int w)
        : name(n), value(0), width(w)     // initializer list
    {
        // Constructor body runs after all members are initialized
        std::cout << "Signal '" << name << "' created, width=" << width << "\n";
    }

    // Member function — has access to all member variables via 'this'
    void write(int v) {
        value = v;
        std::cout << name << " <= " << value << "\n";
    }

    int read() const {    // 'const' = this function doesn't modify the object
        return value;
    }
};

// Struct is identical to class but members are public by default
struct Pair {
    int a;
    int b;

    Pair(int a, int b) : a(a), b(b) {}   // 'a(a)': member 'a' initialized from parameter 'a'

    int sum() const { return a + b; }
};

int main() {
    // Create objects — constructor runs immediately
    Signal clk("clk", 1);
    Signal bus("bus", 32);

    clk.write(1);
    bus.write(0xDEAD);

    std::cout << "clk = " << clk.read() << "\n";
    std::cout << "bus = " << bus.read() << "\n";

    // Pair: struct works identically to class
    Pair p(3, 4);
    std::cout << "pair sum = " << p.sum() << "\n";

    // Key point: the member initializer list runs before the body.
    // If Signal had a member that was itself an object (like 'sc_module' in SystemC),
    // you MUST initialize it in the list — you cannot assign to it in the body.

    return 0;
}
