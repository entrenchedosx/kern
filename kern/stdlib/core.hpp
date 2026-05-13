/* *
 * kern/stdlib/core.hpp - Core Standard Library
 * 
 * Essential functions for Kern programs.
 * Mathematical operations, string manipulation, I/O functions.
 */

#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstdlib>

// Forward declaration for VM
namespace kern {
    class VM;
}

namespace kern::stdlib {

// Mathematical Functions
class Math {
public:
    static double abs(double x) { return std::fabs(x); }
    static double sqrt(double x) { return std::sqrt(x); }
    static double pow(double x, double y) { return std::pow(x, y); }
    static double sin(double x) { return std::sin(x); }
    static double cos(double x) { return std::cos(x); }
    static double tan(double x) { return std::tan(x); }
    static double log(double x) { return std::log(x); }
    static double log10(double x) { return std::log10(x); }
    static double exp(double x) { return std::exp(x); }
    static double floor(double x) { return std::floor(x); }
    static double ceil(double x) { return std::ceil(x); }
    static double round(double x) { return std::round(x); }
    static int64_t min(int64_t a, int64_t b) { return a < b ? a : b; }
    static int64_t max(int64_t a, int64_t b) { return a > b ? a : b; }
};

// String Functions
class String {
public:
    static int64_t length(const std::string& s) { return s.length(); }
    static std::string substr(const std::string& s, int64_t start, int64_t len) {
        if (start < 0) start = 0;
        if (len < 0) len = 0;
        return s.substr(start, len);
    }
    static std::string concat(const std::string& a, const std::string& b) { return a + b; }
    static int64_t find(const std::string& s, const std::string& substr) {
        auto pos = s.find(substr);
        return pos == std::string::npos ? -1 : pos;
    }
    static std::string to_upper(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }
    static std::string to_lower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }
};

// Array Functions
class Array {
public:
    static int64_t size(const std::vector<std::string>& arr) { return arr.size(); }
    static std::string get(const std::vector<std::string>& arr, int64_t index) {
        if (index < 0 || index >= arr.size()) return "";
        return arr[index];
    }
    static std::vector<std::string> push(const std::vector<std::string>& arr, const std::string& value) {
        std::vector<std::string> result = arr;
        result.push_back(value);
        return result;
    }
    static std::vector<std::string> pop(const std::vector<std::string>& arr) {
        if (arr.empty()) return arr;
        std::vector<std::string> result = arr;
        result.pop_back();
        return result;
    }
};

// I/O Functions
class IO {
public:
    static void println(const std::string& s) {
        std::cout << s << std::endl;
    }
    static void print(const std::string& s) {
        std::cout << s;
    }
    static std::string read_line() {
        std::string line;
        std::getline(std::cin, line);
        return line;
    }
};

// Type Conversion Functions
class Convert {
public:
    static std::string to_string(int64_t i) { return std::to_string(i); }
    static std::string to_string(double d) { return std::to_string(d); }
    static std::string to_string(bool b) { return b ? "true" : "false"; }
    static int64_t to_int(const std::string& s) {
        try { return std::stoll(s); } catch (...) { return 0; }
    }
    static double to_double(const std::string& s) {
        try { return std::stod(s); } catch (...) { return 0.0; }
    }
    static bool to_bool(const std::string& s) {
        return !s.empty() && s != "0" && s != "false" && s != "null";
    }
};

// System Functions
class System {
public:
    static int64_t clock() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    static void exit(int64_t code) {
        std::exit(code);
    }
};

// Initialize standard library functions
void initializeStandardLibrary(VM& vm);

} // namespace kern::stdlib
