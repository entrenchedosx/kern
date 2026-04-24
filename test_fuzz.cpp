/* *
 * test_fuzz.cpp - Fuzz Testing Runner
 * 
 * Runs thousands of random programs to find crashes.
 * Usage: ./test_fuzz [iterations] [seed]
 */
#include <iostream>
#include <cstdlib>
#include "kern/testing/fuzz_tester.hpp"

using namespace kern::testing;

int main(int argc, char* argv[]) {
    int iterations = (argc > 1) ? std::atoi(argv[1]) : 10000;
    int seed = (argc > 2) ? std::atoi(argv[2]) : 42;
    
    std::cout << "========================================\n";
    std::cout << "Kern Fuzz Test Runner\n";
    std::cout << "========================================\n";
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Seed: " << seed << "\n\n";
    
    FuzzTester fuzzer(iterations, seed);
    auto result = fuzzer.run();
    fuzzer.printReport(result);
    
    // Return 0 if no crashes, 1 if crashes found
    return (result.crashes > 0) ? 1 : 0;
}
