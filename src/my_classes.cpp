#include <iostream>

class Aclass {
    public:
        std::string name;
        Aclass(std::string the_name) : name(the_name) {
            std::cout << "Aclass constructor run\n";
        };
        int change_name(std::string new_name) {
            name = new_name;
            return 0;
        };
};

class Bclass : public Aclass {
    public:
        std::string name;
        Bclass(std::string the_name) : name(the_name) {
            std::cout << "Bclass constructor run\n";
        };
    };

void func(int &x) {
    x = 42;
};

int main() {
    int *x = new int(0);  // allocate an int on the heap, initialized to 0
    int y = 56;

    Aclass a("my name");
    std::cout << "a.name = " << a.name << "\n";
    std::cout << "x = " << *x << "\n";
    func(*x);  // pass the int by reference to func, which modifies it
    func(y);   // pass y by reference to func, which modifies it
    std::cout << "x = " << *x << "\n";
    std::cout << "y = " << y << "\n";
};
