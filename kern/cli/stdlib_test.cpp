/* *
 * kern/cli/stdlib_test.cpp - Standard Library Test Program
 * 
 * Tests the standard library functions integrated with the VM.
 */

#include <iostream>
#include <fstream>
#include <string>
#include "../compiler/minimal_codegen.hpp"
#include "../core/bytecode/bytecode.hpp"
#include "../runtime/vm/vm.hpp"
#include "../stdlib/core.hpp"

int main() {
    std::cout << "=== Standard Library Test ===" << std::endl;
    
    try {
        // Test basic standard library functionality
        kern::VM vm;
        
        // Initialize standard library
        kern::stdlib::initializeStandardLibrary(vm);
        std::cout << "Standard library initialized" << std::endl;
        
        // Create a simple bytecode that tests builtin functions
        kern::Bytecode code;
        
        // Test builtin 10 (math_abs) with absolute value of -5
        code.push_back({kern::Opcode::CONST_I64, (int64_t)0});  // Load constant "-5"
        code.push_back({kern::Opcode::BUILTIN, (size_t)10});     // Call abs builtin
        code.push_back({kern::Opcode::PRINT, (int64_t)0});       // Print result
        
        // Test builtin 17 (string_length) with "Hello"
        code.push_back({kern::Opcode::CONST_STR, (size_t)0});     // Load string "Hello"
        code.push_back({kern::Opcode::BUILTIN, (size_t)17});     // Call length builtin
        code.push_back({kern::Opcode::PRINT, (int64_t)0});       // Print result
        
        // Test builtin 22 (println) with "Standard Library Test"
        code.push_back({kern::Opcode::CONST_STR, (size_t)1});     // Load string "Standard Library Test"
        code.push_back({kern::Opcode::BUILTIN, (size_t)22});     // Call println builtin
        
        // Test builtin 26 (system_clock)
        code.push_back({kern::Opcode::BUILTIN, (size_t)26});     // Call clock builtin
        code.push_back({kern::Opcode::PRINT, (int64_t)0});       // Print result
        
        code.push_back({kern::Opcode::HALT, (int64_t)0});
        
        std::cout << "Created " << code.size() << " test instructions" << std::endl;
        
        // Set up constants
        std::vector<std::string> stringConstants = {"-5", "Hello", "Standard Library Test"};
        vm.setStringConstants(stringConstants);
        
        std::vector<kern::Value> valueConstants;
        valueConstants.push_back(kern::Value::fromInt(-5));
        vm.setValueConstants(valueConstants);
        
        // Display bytecode
        std::cout << "\nBytecode:" << std::endl;
        for (size_t i = 0; i < code.size(); i++) {
            const auto& instr = code[i];
            std::cout << i << ": " << opcodeName(instr.op);
            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                    std::cout << " " << arg;
                } else if constexpr (std::is_same_v<T, double>) {
                    std::cout << " " << arg;
                } else if constexpr (std::is_same_v<T, std::string>) {
                    std::cout << " \"" << arg << "\"";
                } else if constexpr (std::is_same_v<T, size_t>) {
                    std::cout << " " << arg;
                } else if constexpr (std::is_same_v<T, std::pair<size_t, size_t>>) {
                    std::cout << " (" << arg.first << "," << arg.second << ")";
                }
            }, instr.operand);
            std::cout << std::endl;
        }
        
        // Execute the test
        std::cout << "\n=== VM Execution ===" << std::endl;
        vm.setBytecode(code);
        vm.run();
        
        std::cout << "Standard library test completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
