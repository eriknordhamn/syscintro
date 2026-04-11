// C++ Primer 2: Inheritance, virtual functions, pure virtual (interfaces)
// =======================================================================
// Concepts:
//   - Inheritance: a class can extend another class
//   - Virtual function: can be overridden by a derived class
//   - Pure virtual (= 0): makes the class abstract — cannot be instantiated
//   - Abstract class as interface: declares what a class must do, not how
//
// SystemC connection:
//   SC_MODULE(Foo) : struct Foo : sc_module — Foo inherits from sc_module
//   sc_interface uses pure virtual methods — any channel implementing it
//   must provide those methods (like bus_if in demo4).
//   'virtual public sc_interface' uses virtual inheritance to avoid
//   duplicate base class copies in diamond inheritance.

#include <iostream>
#include <string>

// ---- Abstract interface (pure virtual = 0) ----
// Cannot instantiate this directly — it only declares a contract.
class Writable {
public:
    virtual void write(int value) = 0;   // pure virtual — must be overridden
    virtual int  read()  const   = 0;
    virtual ~Writable() = default;       // virtual destructor — good practice
};

// ---- Base class ----
class Port : public Writable {           // Port inherits from Writable
public:
    std::string name;

    Port(std::string n) : name(n) {
        std::cout << "Port '" << name << "' constructed\n";
    }

    // Derived class must implement all pure virtuals
    void write(int value) override {
        stored = value;
        std::cout << name << ".write(" << value << ")\n";
    }

    int read() const override {
        return stored;
    }

protected:
    int stored = 0;     // only accessible by this class and derived classes
};

// ---- Derived class — extends Port with extra behavior ----
class LoggedPort : public Port {
public:
    int write_count = 0;

    LoggedPort(std::string n) : Port(n) {}   // passes name up to Port's constructor

    void write(int value) override {
        write_count++;
        std::cout << "[write #" << write_count << "] ";
        Port::write(value);    // call the parent's write explicitly
    }
};

// ---- Function that works on any Writable ----
// This is polymorphism: we don't need to know the concrete type
void drive(Writable& w, int val) {
    w.write(val);
}

int main() {
    Port       p("plain_port");
    LoggedPort lp("logged_port");

    drive(p,  42);       // works on Port
    drive(lp, 42);       // works on LoggedPort — calls the overridden version
    drive(lp, 99);

    std::cout << "logged_port write count = " << lp.write_count << "\n";

    // Writable& w = p;     // OK — reference to base
    // Writable  w2;        // ERROR — cannot instantiate abstract class

    return 0;
}
