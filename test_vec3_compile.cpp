// Quick compile test for Vec3
#include "kern/core/bytecode/value.hpp"
#include <iostream>

int main() {
    kern::Value v = kern::Value::fromVec3(1.0, 2.0, 3.0);
    std::cout << "Vec3 created successfully" << std::endl;
    std::cout << "Type: " << (int)v.type << std::endl;
    std::cout << "toString: " << v.toString() << std::endl;
    return 0;
}
