// C++ Primer 3: Templates
// ========================
// Concepts:
//   - Function template: one function that works for multiple types
//   - Class template: one class parameterized by a type
//   - Template argument: the type inside <> at the call/instantiation site
//
// SystemC connection:
//   sc_signal<bool>, sc_signal<int>, sc_fifo<Packet> are all the SAME class
//   template instantiated with different types. When you write:
//       sc_fifo_in<Packet> in;
//   <Packet> is the template argument — it tells the fifo what type to carry.
//   simple_initiator_socket<Cpu> socket — <Cpu> is the template argument
//   telling the socket which module owns it.

#include <iostream>
#include <string>

// ---- Function template ----
// The compiler generates a concrete version for each type T you call it with.
template<typename T>
void print_value(const std::string& name, T value) {
    std::cout << name << " = " << value << "\n";
}

// ---- Class template ----
// Like sc_signal<T>: a generic container that can hold any type.
template<typename T>
class Channel {
public:
    std::string name;

    Channel(std::string n) : name(n), value_() {}

    void write(T v) {
        value_ = v;
        std::cout << name << " <= " << value_ << "\n";
    }

    T read() const {
        return value_;
    }

private:
    T value_;      // T is the placeholder — replaced at compile time
};

// ---- Template with multiple parameters ----
// Like std::pair<A,B>
template<typename Key, typename Val>
struct Entry {
    Key key;
    Val val;
    Entry(Key k, Val v) : key(k), val(v) {}
};

int main() {
    // Function template: T is deduced from the argument
    print_value("clk",  true);
    print_value("addr", 0xFF);
    print_value("name", std::string("foo"));

    // Class template: T must be specified explicitly in <>
    Channel<bool> clk_sig("clk");       // like sc_signal<bool>
    Channel<int>  data_sig("data");     // like sc_signal<int>

    clk_sig.write(true);
    data_sig.write(42);

    std::cout << "data = " << data_sig.read() << "\n";

    // Multiple template parameters
    Entry<std::string, int> e("width", 32);
    std::cout << e.key << " = " << e.val << "\n";

    // Key point: Channel<bool> and Channel<int> are DIFFERENT types
    // generated from the same template — just like sc_signal<bool>
    // and sc_signal<int> are different instantiations of the same template.

    return 0;
}
