/* *
 * kern/cli/compiler_test.cpp - Test the existing Kern compiler
 */

#include <iostream>
#include <fstream>
#include <string>
#include "../compiler/minimal_codegen.hpp"
#include "../core/bytecode/bytecode.hpp"
#include "../runtime/vm/vm.hpp"

int main() {
    std::cout << "=== Kern Compiler Test ===" << std::endl;
    
    try {
        // Read test program
        std::ifstream file("test_program.kern");
        if (!file.is_open()) {
            std::cout << "Error: Could not open test_program.kern" << std::endl;
            return 1;
        }
        
        std::string source((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        file.close();
        
        std::cout << "Source code:" << std::endl;
        std::cout << source << std::endl;
        
        // Compile to bytecode
        kern::MinimalCodeGen compiler;
        kern::Bytecode bytecode = compiler.compile(source);
        
        std::cout << "\nCompiled " << bytecode.size() << " instructions" << std::endl;
        
        // Display bytecode
        std::cout << "\nBytecode:" << std::endl;
        for (size_t i = 0; i < bytecode.size(); i++) {
            const auto& instr = bytecode[i];
            std::cout << i << ": " << opcodeName(instr.op);
            // Display operand based on type
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
        
        // Execute in VM
        std::cout << "\n=== VM Execution ===" << std::endl;
        
        kern::VM vm;
        vm.setBytecode(bytecode);
        
        // Set string constants in VM
        const auto& constants = compiler.getConstants();
        if (!constants.empty()) {
            std::cout << "Setting string constants: ";
            for (size_t i = 0; i < constants.size(); i++) {
                std::cout << i << "=\"" << constants[i] << "\"";
                if (i < constants.size() - 1) std::cout << ", ";
            }
            std::cout << std::endl;
            vm.setStringConstants(constants);
        }
        
        // Set value constants (numbers)
        std::vector<kern::Value> valueConstants;
        for (const auto& constant : constants) {
            // Try to parse as number first
            try {
                if (constant.find('.') != std::string::npos) {
                    // Float
                    double val = std::stod(constant);
                    valueConstants.push_back(kern::Value::fromFloat(val));
                } else {
                    // Integer
                    int64_t val = std::stoll(constant);
                    valueConstants.push_back(kern::Value::fromInt(val));
                }
            } catch (...) {
                // Not a number, skip
            }
        }
        
        if (!valueConstants.empty()) {
            vm.setValueConstants(valueConstants);
        }
        
        // Run the VM
        vm.run();
        std::cout << "VM execution completed" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
