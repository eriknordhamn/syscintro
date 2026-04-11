// C++ Demo 1: Classes — basics
// =============================
// Topics:
//   - Class definition, public/private access
//   - Constructors: default, parameterized, delegating, initializer lists
//   - Destructors (that actually destruct — freeing a heap buffer)
//   - const member functions
//   - static members
//   - friend functions
//
// Note on destruction:
//   A destructor is the place where YOU free resources the class owns.
//   Members that already manage their own storage (like std::string) clean
//   themselves up automatically after your destructor body runs — you don't
//   need to free them. But raw, owned resources (new[], fopen, socket, ...)
//   only get released if you do it explicitly. Here we own a heap int[]
//   and delete[] it in the destructor, so the cleanup is actually visible.
//
//   Because Counter now owns a raw resource, we DELETE the copy operations
//   to avoid double-free bugs. Proper copy/move handling is demo 4
//   (rule of five / RAII).

#include <cstring>
#include <iostream>
#include <string>

class Counter {
public:
    // Default constructor
    Counter() : Counter("anon", 0) {}     // delegating constructor

    // Parameterized constructor with member init list
    Counter(std::string name, int start)
        : name_(std::move(name)),
          value_(start),
          history_(new int[HISTORY_SIZE]),   // <-- heap allocation
          history_len_(0)
    {
        std::memset(history_, 0, HISTORY_SIZE * sizeof(int));
        instance_count_++;
        std::cout << "  [ctor] " << name_ << " = " << value_
                  << "   (new int[" << HISTORY_SIZE << "] @ " << history_ << ")"
                  << std::endl;
    }

    // Copy operations deleted — see demo 4 for how to do this safely
    Counter(const Counter&)            = delete;
    Counter& operator=(const Counter&) = delete;

    // Destructor: release the heap buffer we own
    ~Counter() {
        std::cout << "  [dtor] " << name_
                  << "   (delete[] @ " << history_ << ")" << std::endl;
        delete[] history_;                   // <-- real destruction
        history_ = nullptr;
        instance_count_--;
    }

    // Non-const member function (mutates state)
    void tick() {
        ++value_;
        if (history_len_ < HISTORY_SIZE)
            history_[history_len_++] = value_;
    }

    // const member function (read-only; required on const objects)
    int  value() const { return value_; }
    const std::string& name() const { return name_; }

    // Static member function + data
    static int alive() { return instance_count_; }

    // Friend: grants non-member function access to private data
    friend std::ostream& operator<<(std::ostream& os, const Counter& c);

private:
    static constexpr int HISTORY_SIZE = 8;

    std::string name_;
    int         value_;
    int*        history_;         // OWNED raw resource — we must delete[]
    int         history_len_;
    static int  instance_count_;  // declaration
};

// Static member definition (required out-of-class)
int Counter::instance_count_ = 0;

std::ostream& operator<<(std::ostream& os, const Counter& c) {
    os << "Counter{" << c.name_ << "=" << c.value_ << " history=[";
    for (int i = 0; i < c.history_len_; i++) {
        os << c.history_[i] << (i + 1 < c.history_len_ ? "," : "");
    }
    return os << "]}";
}

int main() {
    std::cout << "=== C++ Demo 1: Classes ===\n\n";

    std::cout << "-- Construction --\n";
    Counter a;                 // default
    Counter b("cycles", 100);  // parameterized
    std::cout << "  alive=" << Counter::alive() << "\n\n";

    std::cout << "-- Methods --\n";
    b.tick(); b.tick(); b.tick();
    std::cout << "  " << b << "\n\n";

    std::cout << "-- const correctness --\n";
    const Counter c("frozen", 42);
    std::cout << "  " << c.name() << "=" << c.value() << "\n";
    // c.tick();   // ERROR: tick() is not const, c is const
    std::cout << "  (c.tick() would not compile — const object)\n\n";

    std::cout << "-- Scope and destruction order --\n";
    {
        Counter temp("temp", 7);
        std::cout << "  inner scope: alive=" << Counter::alive() << "\n";
    }   // temp destroyed here — dtor frees its history[]
    std::cout << "  after inner scope: alive=" << Counter::alive() << "\n\n";

    std::cout << "-- End of main (remaining objects destroyed in reverse) --\n";
    return 0;
}
