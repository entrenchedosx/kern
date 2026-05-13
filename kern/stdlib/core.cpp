/* *
 * kern/stdlib/core.cpp - Core Standard Library Implementation
 * 
 * Registers standard library functions with the VM's native binding system.
 */

#include "core.hpp"
#include "../runtime/vm/vm.hpp"

namespace kern::stdlib {

// Native function wrappers for VM integration
class NativeWrappers {
public:
    // Math functions
    static Value math_abs(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        double x = args[0]->asFloat();
        return Value::fromFloat(Math::abs(x));
    }
    
    static Value math_sqrt(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        double x = args[0]->asFloat();
        return Value::fromFloat(Math::sqrt(x));
    }
    
    static Value math_pow(VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        double x = args[0]->asFloat();
        double y = args[1]->asFloat();
        return Value::fromFloat(Math::pow(x, y));
    }
    
    static Value math_sin(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        double x = args[0]->asFloat();
        return Value::fromFloat(Math::sin(x));
    }
    
    static Value math_cos(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        double x = args[0]->asFloat();
        return Value::fromFloat(Math::cos(x));
    }
    
    static Value math_min(VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int64_t a = args[0]->asInt();
        int64_t b = args[1]->asInt();
        return Value::fromInt(Math::min(a, b));
    }
    
    static Value math_max(VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int64_t a = args[0]->asInt();
        int64_t b = args[1]->asInt();
        return Value::fromInt(Math::max(a, b));
    }
    
    // String functions
    static Value string_length(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string s = args[0]->asString();
        return Value::fromInt(String::length(s));
    }
    
    static Value string_concat(VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        std::string a = args[0]->asString();
        std::string b = args[1]->asString();
        return Value::fromString(String::concat(a, b));
    }
    
    static Value string_find(VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        std::string s = args[0]->asString();
        std::string substr = args[1]->asString();
        return Value::fromInt(String::find(s, substr));
    }
    
    static Value string_upper(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string s = args[0]->asString();
        return Value::fromString(String::to_upper(s));
    }
    
    static Value string_lower(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string s = args[0]->asString();
        return Value::fromString(String::to_lower(s));
    }
    
    // I/O functions
    static Value io_println(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string s = args[0]->asString();
        IO::println(s);
        return Value::nil();
    }
    
    static Value io_print(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string s = args[0]->asString();
        IO::print(s);
        return Value::nil();
    }
    
    // Conversion functions
    static Value convert_to_int(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string s = args[0]->asString();
        return Value::fromInt(Convert::to_int(s));
    }
    
    static Value convert_to_double(VM* vm, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string s = args[0]->asString();
        return Value::fromFloat(Convert::to_double(s));
    }
    
    // System functions
    static Value system_clock(VM* vm, std::vector<ValuePtr> args) {
        return Value::fromInt(System::clock());
    }
};

// Initialize standard library functions
void initializeStandardLibrary(VM& vm) {
    // Math functions - start from index 10 to avoid conflicts with existing builtins
    vm.registerBuiltin(10, NativeWrappers::math_abs);     // abs
    vm.registerBuiltin(11, NativeWrappers::math_sqrt);    // sqrt
    vm.registerBuiltin(12, NativeWrappers::math_pow);     // pow
    vm.registerBuiltin(13, NativeWrappers::math_sin);     // sin
    vm.registerBuiltin(14, NativeWrappers::math_cos);     // cos
    vm.registerBuiltin(15, NativeWrappers::math_min);     // min
    vm.registerBuiltin(16, NativeWrappers::math_max);     // max
    
    // String functions
    vm.registerBuiltin(17, NativeWrappers::string_length); // length
    vm.registerBuiltin(18, NativeWrappers::string_concat); // concat
    vm.registerBuiltin(19, NativeWrappers::string_find);   // find
    vm.registerBuiltin(20, NativeWrappers::string_upper); // upper
    vm.registerBuiltin(21, NativeWrappers::string_lower); // lower
    
    // I/O functions
    vm.registerBuiltin(22, NativeWrappers::io_println);   // println
    vm.registerBuiltin(23, NativeWrappers::io_print);     // print
    
    // Conversion functions
    vm.registerBuiltin(24, NativeWrappers::convert_to_int);   // to_int
    vm.registerBuiltin(25, NativeWrappers::convert_to_double); // to_double
    
    // System functions
    vm.registerBuiltin(26, NativeWrappers::system_clock); // clock
}

} // namespace kern::stdlib
