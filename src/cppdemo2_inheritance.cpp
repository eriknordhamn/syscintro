// C++ Demo 2: Inheritance and polymorphism
// ==========================================
// Topics:
//   - Base / derived classes
//   - virtual functions and dynamic dispatch
//   - Pure virtual (abstract) classes / interfaces
//   - override and final
//   - Virtual destructors (essential for base pointers)
//   - Calling base constructors / methods
//   - dynamic_cast

#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ---- Abstract base class (interface) ----
class Shape {
public:
    Shape(std::string name) : name_(std::move(name)) {}

    // Virtual destructor: required when deleting derived via base pointer
    virtual ~Shape() = default;

    // Pure virtual — subclasses MUST implement. Makes Shape abstract.
    virtual double area() const = 0;
    virtual double perimeter() const = 0;

    // Virtual (with default impl) — subclasses MAY override
    virtual void describe() const {
        std::cout << "  " << name_
                  << " area=" << area()
                  << " perim=" << perimeter() << "\n";
    }

    const std::string& name() const { return name_; }

protected:
    std::string name_;   // accessible to derived classes
};

// ---- Concrete derived classes ----
class Circle : public Shape {
public:
    Circle(double r) : Shape("Circle"), r_(r) {}

    double area() const override      { return 3.14159265 * r_ * r_; }
    double perimeter() const override { return 2 * 3.14159265 * r_; }

private:
    double r_;
};

class Rectangle : public Shape {
public:
    Rectangle(double w, double h) : Shape("Rectangle"), w_(w), h_(h) {}

    double area() const override      { return w_ * h_; }
    double perimeter() const override { return 2 * (w_ + h_); }

protected:
    double w_, h_;
};

// Further derivation — Square is a Rectangle with w==h
// "final" prevents further inheritance
class Square final : public Rectangle {
public:
    Square(double s) : Rectangle(s, s) {}

    // Override describe() to add extra info (calls base version too)
    void describe() const override {
        std::cout << "  [Square]\n    ";
        Rectangle::describe();     // explicit base call
    }
};

int main() {
    std::cout << "=== C++ Demo 2: Inheritance ===\n\n";

    // Polymorphism: container of base pointers, mixed derived types
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>(3.0));
    shapes.push_back(std::make_unique<Rectangle>(4.0, 5.0));
    shapes.push_back(std::make_unique<Square>(6.0));

    std::cout << "-- Dynamic dispatch via base pointers --\n";
    for (const auto& s : shapes) {
        s->describe();     // calls the right derived version
    }

    // Total area via virtual dispatch
    double total = 0;
    for (const auto& s : shapes) total += s->area();
    std::cout << "\n  total area = " << total << "\n\n";

    // dynamic_cast: safe downcast, returns nullptr if wrong type
    std::cout << "-- dynamic_cast --\n";
    for (const auto& s : shapes) {
        if (auto* r = dynamic_cast<Rectangle*>(s.get())) {
            std::cout << "  " << s->name() << " IS a Rectangle\n";
            (void)r;
        } else {
            std::cout << "  " << s->name() << " is NOT a Rectangle\n";
        }
    }

    std::cout << "\n-- Destruction (virtual dtor ensures derived dtors run) --\n";
    // unique_ptrs go out of scope here
    return 0;
}
