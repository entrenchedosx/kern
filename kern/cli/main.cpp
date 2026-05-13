#include <iostream>
#include <cstdint>
#include <string>
#include <memory>
#include "../core/compiler/lexer.hpp"
#include "../core/compiler/parser.hpp"
#include "../core/compiler/codegen.hpp"
#include "../core/bytecode/bytecode.hpp"
#include "../runtime/vm/vm.hpp"
#include "../runtime/vm/vec3_builtins.hpp"
#include "../runtime/vm/collection_builtins.hpp"

int main() {
    std::cout << "=== Struct V1 Test ===" << std::endl;
    try {
        kern::VM vm;
        kern::registerVec3Builtins(vm);
        kern::registerCollectionBuiltins(vm);

        // Test 1: Struct definition + instantiation + field access
        std::string source = R"(
struct Entity { x: float, y: float }
let e = Entity(x: 10.0, y: 25.0)
print("e.y =")
print(e.y)
)";

        std::cout << "--- Source ---" << std::endl;
        std::cout << source << std::endl;

        kern::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        
        kern::Parser parser(tokens);
        auto program = parser.parse();
        
        std::cout << "--- Parsed OK, generating bytecode ---" << std::endl;
        
        kern::CodeGenerator cg;
        auto bytecode = cg.generate(std::move(program));

        std::cout << "--- Bytecode (" << bytecode.size() << " instructions) ---" << std::endl;
        auto& consts = cg.getConstants();
        for (size_t i = 0; i < bytecode.size(); ++i) {
            std::cout << "  " << i << ": " << static_cast<int>(bytecode[i].op);
            // Try to display string operands for relevant opcodes
            bool disp = false;
            if (bytecode[i].op == kern::Opcode::CONST_STR || 
                bytecode[i].op == kern::Opcode::STORE_GLOBAL ||
                bytecode[i].op == kern::Opcode::LOAD_GLOBAL ||
                bytecode[i].op == kern::Opcode::SET_FIELD ||
                bytecode[i].op == kern::Opcode::GET_FIELD ||
                bytecode[i].op == kern::Opcode::SET_FUNC_NAME ||
                bytecode[i].op == kern::Opcode::SET_FUNC_PARAM_NAMES) {
                size_t idx = std::get<size_t>(bytecode[i].operand);
                if (idx < consts.size()) { std::cout << " \"" << consts[idx] << "\""; disp = true; }
            }
            if (!disp && bytecode[i].op == kern::Opcode::CONST_I64) {
                std::cout << " " << std::get<int64_t>(bytecode[i].operand);
            }
            if (!disp && bytecode[i].op == kern::Opcode::CONST_F64) {
                std::cout << " " << std::get<double>(bytecode[i].operand);
            }
            if (!disp && bytecode[i].op == kern::Opcode::CALL) {
                std::cout << " argc=" << std::get<size_t>(bytecode[i].operand);
            }
            if (!disp && bytecode[i].op == kern::Opcode::BUILD_ARRAY) {
                std::cout << " n=" << std::get<size_t>(bytecode[i].operand);
            }
            if (!disp && (bytecode[i].op == kern::Opcode::JMP || bytecode[i].op == kern::Opcode::JMP_IF_FALSE)) {
                std::cout << " ->" << std::get<size_t>(bytecode[i].operand);
            }
            std::cout << std::endl;
        }

        vm.setStringConstants(cg.getConstants());
        vm.setValueConstants(cg.getValueConstants());
        vm.setBytecode(bytecode);
        std::cout << "--- Running VM ---" << std::endl;
        vm.run();
        std::cout << "--- VM finished ---" << std::endl;

        // Test 2: Result type with ? (Try) operator
        std::cout << std::endl << "=== Result ? Operator Test ===" << std::endl;
        std::string source2 =
            "def divide(a, b) {\n"
            "    if (b == 0) { return err(\"Div by zero\"); }\n"
            "    return ok(a / b);\n"
            "}\n"
            "def do_math() {\n"
            "    let val = divide(10, 0)?;\n"
            "    return ok(val + 5);\n"
            "}\n"
            "let res = do_math();\n"
            "print(res.error);\n";

        std::cout << "--- Source ---" << std::endl;
        std::cout << source2 << std::endl;

        kern::VM vm2;
        kern::registerVec3Builtins(vm2);
        kern::registerCollectionBuiltins(vm2);

        kern::Lexer lexer2(source2);
        auto tokens2 = lexer2.tokenize();

        kern::Parser parser2(tokens2);
        auto program2 = parser2.parse();

        std::cout << "--- Parsed OK, generating bytecode ---" << std::endl;

        kern::CodeGenerator cg2;
        auto bytecode2 = cg2.generate(std::move(program2));

        std::cout << "--- Bytecode (" << bytecode2.size() << " instructions) ---" << std::endl;
        auto& consts2 = cg2.getConstants();
        for (size_t i = 0; i < bytecode2.size(); ++i) {
            std::cout << "  " << i << ": " << static_cast<int>(bytecode2[i].op);
            bool disp = false;
            if (bytecode2[i].op == kern::Opcode::CONST_STR ||
                bytecode2[i].op == kern::Opcode::STORE_GLOBAL ||
                bytecode2[i].op == kern::Opcode::LOAD_GLOBAL ||
                bytecode2[i].op == kern::Opcode::SET_FIELD ||
                bytecode2[i].op == kern::Opcode::GET_FIELD ||
                bytecode2[i].op == kern::Opcode::SET_FUNC_NAME ||
                bytecode2[i].op == kern::Opcode::SET_FUNC_PARAM_NAMES) {
                size_t idx = std::get<size_t>(bytecode2[i].operand);
                if (idx < consts2.size()) { std::cout << " \"" << consts2[idx] << "\""; disp = true; }
            }
            if (!disp && bytecode2[i].op == kern::Opcode::CONST_I64) {
                std::cout << " " << std::get<int64_t>(bytecode2[i].operand);
            }
            if (!disp && bytecode2[i].op == kern::Opcode::CONST_F64) {
                std::cout << " " << std::get<double>(bytecode2[i].operand);
            }
            if (!disp && bytecode2[i].op == kern::Opcode::CALL) {
                std::cout << " argc=" << std::get<size_t>(bytecode2[i].operand);
            }
            if (!disp && (bytecode2[i].op == kern::Opcode::JMP ||
                          bytecode2[i].op == kern::Opcode::JMP_IF_TRUE ||
                          bytecode2[i].op == kern::Opcode::JMP_IF_FALSE)) {
                std::cout << " ->" << std::get<size_t>(bytecode2[i].operand);
            }
            std::cout << std::endl;
        }

        vm2.setStringConstants(cg2.getConstants());
        vm2.setValueConstants(cg2.getValueConstants());
        vm2.setBytecode(bytecode2);
        std::cout << "--- Running VM ---" << std::endl;
        vm2.run();
        std::cout << "--- VM finished ---" << std::endl;

        // Test 3: Grand Unification — all features together
        std::cout << std::endl << "=== Grand Unification Test ===" << std::endl;
        std::string source3 =
            "struct Entity { id: int, pos: vec3 }\n"
            "def run_frame() {\n"
            "    let entities = [];\n"
            "    let v = vec3_new(1.0, 2.0, 3.0);\n"
            "    print(v);\n"
            "    entities.push(Entity(id: 1, pos: v));\n"
            "    print(entities.len());\n"
            "    return ok(entities.pop());\n"
            "}\n"
            "let result = run_frame();\n"
            "print(\"Success\");\n";

        std::cout << "--- Source ---" << std::endl;
        std::cout << source3 << std::endl;

        kern::VM vm3;
        kern::registerVec3Builtins(vm3);
        // Vec3 builtins use indices 0-8, so collection builtins must start at 9
        kern::registerCollectionBuiltins(vm3, 9);

        kern::Lexer lexer3(source3);
        auto tokens3 = lexer3.tokenize();

        kern::Parser parser3(tokens3);
        auto program3 = parser3.parse();

        std::cout << "--- Parsed OK, generating bytecode ---" << std::endl;

        kern::CodeGenerator cg3;
        // Register known global functions for UFCS desugaring (collections)
        cg3.addKnownGlobalFunction("len");
        cg3.addKnownGlobalFunction("push");
        cg3.addKnownGlobalFunction("pop");
        cg3.addKnownGlobalFunction("remove");
        // Also register vec3 builtins so obj.method() desugaring works for them too
        cg3.addKnownGlobalFunction("vec3_new");
        cg3.addKnownGlobalFunction("vec3_add");
        cg3.addKnownGlobalFunction("vec3_sub");
        cg3.addKnownGlobalFunction("vec3_dot");
        cg3.addKnownGlobalFunction("vec3_normalize");
        auto bytecode3 = cg3.generate(std::move(program3));

        std::cout << "--- Bytecode (" << bytecode3.size() << " instructions) ---" << std::endl;
        auto& consts3 = cg3.getConstants();
        for (size_t i = 0; i < bytecode3.size(); ++i) {
            std::cout << "  " << i << ": " << static_cast<int>(bytecode3[i].op);
            bool disp = false;
            if (bytecode3[i].op == kern::Opcode::CONST_STR ||
                bytecode3[i].op == kern::Opcode::STORE_GLOBAL ||
                bytecode3[i].op == kern::Opcode::LOAD_GLOBAL ||
                bytecode3[i].op == kern::Opcode::SET_FIELD ||
                bytecode3[i].op == kern::Opcode::GET_FIELD ||
                bytecode3[i].op == kern::Opcode::SET_FUNC_NAME ||
                bytecode3[i].op == kern::Opcode::SET_FUNC_PARAM_NAMES) {
                size_t idx = std::get<size_t>(bytecode3[i].operand);
                if (idx < consts3.size()) { std::cout << " \"" << consts3[idx] << "\""; disp = true; }
            }
            if (!disp && (bytecode3[i].op == kern::Opcode::CONST_I64 ||
                          bytecode3[i].op == kern::Opcode::LOAD ||
                          bytecode3[i].op == kern::Opcode::STORE)) {
                std::cout << " " << std::get<int64_t>(bytecode3[i].operand);
                disp = true;
            }
            if (!disp && bytecode3[i].op == kern::Opcode::CONST_F64) {
                std::cout << " " << std::get<double>(bytecode3[i].operand);
                disp = true;
            }
            if (!disp && bytecode3[i].op == kern::Opcode::CALL) {
                std::cout << " argc=" << std::get<size_t>(bytecode3[i].operand);
                disp = true;
            }
            if (!disp && bytecode3[i].op == kern::Opcode::BUILD_ARRAY) {
                std::cout << " n=" << std::get<size_t>(bytecode3[i].operand);
                disp = true;
            }
            if (!disp && (bytecode3[i].op == kern::Opcode::JMP ||
                          bytecode3[i].op == kern::Opcode::JMP_IF_TRUE ||
                          bytecode3[i].op == kern::Opcode::JMP_IF_FALSE)) {
                std::cout << " ->" << std::get<size_t>(bytecode3[i].operand);
                disp = true;
            }
            std::cout << std::endl;
        }

        vm3.setStringConstants(cg3.getConstants());
        vm3.setValueConstants(cg3.getValueConstants());
        vm3.setBytecode(bytecode3);
        std::cout << "--- Running VM ---" << std::endl;
        vm3.run();
        std::cout << "--- VM finished ---" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
