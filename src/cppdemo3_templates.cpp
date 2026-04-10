// C++ Demo 3: Templates
// ======================
// Topics:
//   - Function templates
//   - Class templates
//   - Multiple type parameters
//   - Non-type template parameters
//   - Template specialization (full and partial)
//   - Variadic templates (parameter packs)
//   - Type deduction

#include <iostream>
#include <string>
#include <vector>

// ---- Function template ----
// Works for any type T that supports operator<
template <typename T>
const T& max_of(const T& a, const T& b) {
    return (a < b) ? b : a;
}

// ---- Function template with multiple parameters + auto return ----
template <typename A, typename B>
auto add(const A& a, const B& b) -> decltype(a + b) {
    return a + b;
}

// ---- Class template: a simple fixed-capacity stack ----
// Non-type template parameter: N is a compile-time constant int
template <typename T, int N>
class Stack {
public:
    Stack() : top_(0) {}

    bool push(const T& v) {
        if (top_ >= N) return false;
        data_[top_++] = v;
        return true;
    }

    bool pop(T& out) {
        if (top_ == 0) return false;
        out = data_[--top_];
        return true;
    }

    int  size()     const { return top_; }
    int  capacity() const { return N; }
    bool empty()    const { return top_ == 0; }

private:
    T   data_[N];
    int top_;
};

// ---- Full specialization: a different impl for Stack<bool, N> ----
// (toy example — pretend we pack bools into an int bitmap)
template <int N>
class Stack<bool, N> {
public:
    Stack() : top_(0), bits_(0) {
        static_assert(N <= 32, "bitmap stack limited to 32 bits");
    }

    bool push(bool v) {
        if (top_ >= N) return false;
        if (v) bits_ |=  (1u << top_);
        else   bits_ &= ~(1u << top_);
        top_++;
        return true;
    }

    bool pop(bool& out) {
        if (top_ == 0) return false;
        --top_;
        out = (bits_ >> top_) & 1u;
        return true;
    }

    int size() const { return top_; }

private:
    int      top_;
    unsigned bits_;
};

// ---- Variadic template: print any number of args ----
// Base case
void print_all() { std::cout << "\n"; }

// Recursive case: peel off one arg, recurse on the rest
template <typename First, typename... Rest>
void print_all(const First& first, const Rest&... rest) {
    std::cout << first << " ";
    print_all(rest...);      // parameter pack expansion
}

// ---- Variadic sum using fold expression (C++17) ----
template <typename... Args>
auto sum(Args... args) {
    return (args + ...);     // fold expression: (a1 + (a2 + (a3 + ...)))
}

int main() {
    std::cout << "=== C++ Demo 3: Templates ===\n\n";

    std::cout << "-- Function templates --\n";
    std::cout << "  max_of(3, 7)       = " << max_of(3, 7) << "\n";
    std::cout << "  max_of(2.5, 1.8)   = " << max_of(2.5, 1.8) << "\n";
    std::cout << "  max_of(\"ab\",\"xy\") = "
              << max_of(std::string("ab"), std::string("xy")) << "\n";
    std::cout << "  add(1, 2.5)        = " << add(1, 2.5) << "\n\n";

    std::cout << "-- Class template: Stack<int, 4> --\n";
    Stack<int, 4> s;
    s.push(10); s.push(20); s.push(30);
    std::cout << "  size=" << s.size() << " capacity=" << s.capacity() << "\n";
    int v;
    while (s.pop(v)) std::cout << "  popped " << v << "\n";

    std::cout << "\n-- Class template: Stack<std::string, 2> --\n";
    Stack<std::string, 2> ss;
    ss.push("hello");
    ss.push("world");
    std::string str;
    while (ss.pop(str)) std::cout << "  popped " << str << "\n";

    std::cout << "\n-- Specialized Stack<bool, 8> (bitmap impl) --\n";
    Stack<bool, 8> bs;
    bs.push(true); bs.push(false); bs.push(true); bs.push(true);
    std::cout << "  size=" << bs.size() << "\n";
    bool b;
    while (bs.pop(b)) std::cout << "  popped " << b << "\n";

    std::cout << "\n-- Variadic templates --\n";
    std::cout << "  print_all: ";
    print_all(1, 2.5, "three", 'x', std::string("five"));
    std::cout << "  sum(1,2,3,4,5) = " << sum(1, 2, 3, 4, 5) << "\n";
    std::cout << "  sum(1.5, 2.5)  = " << sum(1.5, 2.5) << "\n";

    return 0;
}
