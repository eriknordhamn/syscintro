// C++ Demo 1: Classes — basics
// =============================
// Topics:
//   - Class definition, public/private access
//   - Constructors: default, parameterized, delegating, initializer lists
//   - Destructors
//   - const member functions
//   - static members
//   - friend functions

#include <iostream>
#include <string>

class Counter {
public:
    // Default constructor
    Counter() : Counter("anon", 0) {}     // delegating constructor

    // Parameterized constructor with member init list
    Counter(std::string name, int start)
        : name_(std::move(name)), value_(start)
    {
        instance_count_++;
        std::cout << "  [ctor] " << name_ << " = " << value_ << std::endl;
    }

    // Destructor
    ~Counter() {
        instance_count_--;
        std::cout << "  [dtor] " << name_ << std::endl;
    }

    // Non-const member function (mutates state)
    void tick() { ++value_; }

    // const member function (read-only; required on const objects)
    int  value() const { return value_; }
    const std::string& name() const { return name_; }

    // Static member function + data
    static int alive() { return instance_count_; }

    // Friend: grants non-member function access to private data
    friend std::ostream& operator<<(std::ostream& os, const Counter& c);

private:
    std::string name_;
    int         value_;
    static int  instance_count_;   // declaration
};

// Static member definition (required out-of-class)
int Counter::instance_count_ = 0;

std::ostream& operator<<(std::ostream& os, const Counter& c) {
    return os << "Counter{" << c.name_ << "=" << c.value_ << "}";
}

int main() {
    std::cout << "=== C++ Demo 1: Classes ===\n\n";

    std::cout << "-- Construction --\n";
    Counter a;                 // default
    Counter b("cycles", 100);  // parameterized
    std::cout << "  alive=" << Counter::alive() << "\n\n";

    std::cout << "-- Methods --\n";
    b.tick(); b.tick(); b.tick();
    std::cout << "  " << b << " after 3 ticks (value=" << b.value() << ")\n\n";

    std::cout << "-- const correctness --\n";
    const Counter c("frozen", 42);
    std::cout << "  " << c.name() << "=" << c.value() << "\n";
    // c.tick();   // ERROR: tick() is not const, c is const
    std::cout << "  (c.tick() would not compile — const object)\n\n";

    std::cout << "-- Scope and destruction order --\n";
    {
        Counter temp("temp", 7);
        std::cout << "  inner scope: alive=" << Counter::alive() << "\n";
    }   // temp destroyed here
    std::cout << "  after inner scope: alive=" << Counter::alive() << "\n\n";

    std::cout << "-- End of main (remaining objects destroyed in reverse) --\n";
    return 0;
}
