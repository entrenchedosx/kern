/* *
 * kern/testing/diagnostics.hpp - Debugging and Diagnostics Tools
 * 
 * Tools for understanding the system:
 * - IR disassembler (before/after optimization)
 * - Bytecode disassembler
 * - Execution tracer
 * - AST/CFG visualizers
 */
#pragma once

#include "../ir/linear_ir.hpp"
#include "../runtime/vm_minimal.hpp"
#include <iostream>
#include <iomanip>

namespace kern {
namespace diagnostics {

// ============================================================================
// IR Disassembler
// ============================================================================

class IRDisassembler {
public:
    static void disassemble(const ir::IrFunction& func, std::ostream& out = std::cout) {
        out << "\n=== IR: " << func.name << " ===\n";
        out << "Registers: " << func.registerCount << "\n";
        out << "Parameters: " << func.paramCount << "\n";
        out << "Instructions: " << func.instructions.size() << "\n";
        out << "\n";
        
        for (size_t i = 0; i < func.instructions.size(); i++) {
            const auto& inst = func.instructions[i];
            out << std::setw(4) << i << ": ";
            disassembleInstruction(inst, out);
            out << "\n";
        }
        
        out << "\n";
    }
    
    static void disassemble(const ir::IrProgram& prog, std::ostream& out = std::cout) {
        out << "\n========== IR PROGRAM ==========\n";
        for (const auto& func : prog.functions) {
            disassemble(func, out);
        }
    }
    
    static void disassembleBeforeAfter(const ir::IrFunction& before, 
                                        const ir::IrFunction& after,
                                        std::ostream& out = std::cout) {
        out << "\n========== OPTIMIZATION REPORT ==========\n";
        
        disassemble(before, out);
        
        int reduction = 0;
        if (before.instructions.size() > 0) {
            reduction = (int)((1.0 - (double)after.instructions.size() / 
                             before.instructions.size()) * 100);
        }
        
        out << "\n>>> OPTIMIZED (" << reduction << "% reduction) >>>\n";
        
        disassemble(after, out);
        
        // Show specific changes
        out << "\n=== CHANGES ===\n";
        if (before.instructions.size() != after.instructions.size()) {
            out << "Instructions: " << before.instructions.size() 
                << " -> " << after.instructions.size() << "\n";
        }
        if (before.registerCount != after.registerCount) {
            out << "Registers: " << before.registerCount 
                << " -> " << after.registerCount << "\n";
        }
    }
    
private:
    static void disassembleInstruction(const ir::IrInstr& inst, std::ostream& out) {
        out << std::left << std::setw(12) << irOpToString(inst.op);
        
        if (inst.dest != ir::NO_REG) {
            out << " r" << inst.dest;
        } else {
            out << "     ";
        }
        
        if (!inst.srcA.isNil()) {
            out << ", " << valueToString(inst.srcA);
        }
        
        if (!inst.srcB.isNil()) {
            out << ", " << valueToString(inst.srcB);
        }
        
        if (inst.offset != 0) {
            out << " [offset=" << inst.offset << "]";
        }
        
        if (!inst.name.empty()) {
            out << " \"" << inst.name << "\"";
        }
    }
    
    static std::string irOpToString(ir::IrOp op) {
        switch (op) {
            case ir::IrOp::LOAD_CONST: return "LOAD_CONST";
            case ir::IrOp::LOAD_LOCAL: return "LOAD_LOCAL";
            case ir::IrOp::STORE_LOCAL: return "STORE_LOCAL";
            case ir::IrOp::LOAD_GLOBAL: return "LOAD_GLOBAL";
            case ir::IrOp::STORE_GLOBAL: return "STORE_GLOBAL";
            case ir::IrOp::ADD: return "ADD";
            case ir::IrOp::SUB: return "SUB";
            case ir::IrOp::MUL: return "MUL";
            case ir::IrOp::DIV: return "DIV";
            case ir::IrOp::MOD: return "MOD";
            case ir::IrOp::NEG: return "NEG";
            case ir::IrOp::EQ: return "EQ";
            case ir::IrOp::LT: return "LT";
            case ir::IrOp::LE: return "LE";
            case ir::IrOp::GT: return "GT";
            case ir::IrOp::GE: return "GE";
            case ir::IrOp::NOT: return "NOT";
            case ir::IrOp::AND: return "AND";
            case ir::IrOp::OR: return "OR";
            case ir::IrOp::JUMP: return "JUMP";
            case ir::IrOp::JUMP_IF_TRUE: return "JUMP_IF_TRUE";
            case ir::IrOp::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
            case ir::IrOp::CALL: return "CALL";
            case ir::IrOp::RETURN: return "RETURN";
            case ir::IrOp::PRINT: return "PRINT";
            case ir::IrOp::MOVE: return "MOVE";
            case ir::IrOp::NOP: return "NOP";
            default: return "UNKNOWN";
        }
    }
    
    static std::string valueToString(const ir::IrValue& val) {
        if (val.isNil()) return "nil";
        if (val.isConst()) {
            switch (val.type) {
                case ir::IrValue::CONST_INT: return std::to_string(val.intVal);
                case ir::IrValue::CONST_FLOAT: return std::to_string(val.floatVal);
                case ir::IrValue::CONST_BOOL: return val.boolVal ? "true" : "false";
                case ir::IrValue::CONST_STRING: return "\"" + val.stringVal + "\"";
                default: return "?";
            }
        }
        if (val.type == ir::IrValue::REG) {
            return "r" + std::to_string(val.reg);
        }
        return "?";
    }
};

// ============================================================================
// Bytecode Disassembler
// ============================================================================

class BytecodeDisassembler {
public:
    static void disassemble(const Bytecode& code, 
                            const std::vector<std::string>& constants,
                            std::ostream& out = std::cout) {
        out << "\n========== BYTECODE ==========\n";
        out << "Size: " << code.size() << " instructions\n\n";
        
        if (!constants.empty()) {
            out << "=== Constant Pool ===\n";
            for (size_t i = 0; i < constants.size(); i++) {
                out << "[" << i << "] \"" << constants[i] << "\"\n";
            }
            out << "\n";
        }
        
        out << "=== Instructions ===\n";
        
        // Build label map for jumps
        std::map<size_t, std::string> labels;
        int labelCount = 0;
        
        for (size_t i = 0; i < code.size(); i++) {
            const auto& inst = code[i];
            if (inst.op == Instruction::JUMP || inst.op == Instruction::JUMP_IF_FALSE) {
                int target = (int)i + inst.operand;
                if (target >= 0 && (size_t)target < code.size()) {
                    if (labels.count(target) == 0) {
                        labels[target] = "L" + std::to_string(labelCount++);
                    }
                }
            }
        }
        
        // Disassemble
        for (size_t i = 0; i < code.size(); i++) {
            // Print label if any
            if (labels.count(i)) {
                out << labels[i] << ":\n";
            }
            
            out << std::setw(4) << i << ": ";
            out << std::left << std::setw(15) << opToString(code[i].op);
            
            // Print operand
            if (code[i].operand != 0 || needsOperand(code[i].op)) {
                if (code[i].op == Instruction::PUSH_CONST && 
                    (size_t)code[i].operand < constants.size()) {
                    out << "#" << (int)code[i].operand 
                        << " (\"" << constants[code[i].operand] << "\")";
                } else if ((code[i].op == Instruction::JUMP || 
                           code[i].op == Instruction::JUMP_IF_FALSE) &&
                          labels.count(i + code[i].operand)) {
                    out << (int)code[i].operand << " -> " << labels[i + code[i].operand];
                } else {
                    out << (int)code[i].operand;
                }
            }
            
            out << "\n";
        }
        
        out << "\n";
    }
    
private:
    static std::string opToString(Instruction::Op op) {
        switch (op) {
            case Instruction::PUSH_CONST: return "PUSH_CONST";
            case Instruction::PUSH_NIL: return "PUSH_NIL";
            case Instruction::PUSH_TRUE: return "PUSH_TRUE";
            case Instruction::PUSH_FALSE: return "PUSH_FALSE";
            case Instruction::POP: return "POP";
            case Instruction::DUP: return "DUP";
            case Instruction::LOAD_LOCAL: return "LOAD_LOCAL";
            case Instruction::STORE_LOCAL: return "STORE_LOCAL";
            case Instruction::LOAD_GLOBAL: return "LOAD_GLOBAL";
            case Instruction::STORE_GLOBAL: return "STORE_GLOBAL";
            case Instruction::ADD: return "ADD";
            case Instruction::SUB: return "SUB";
            case Instruction::MUL: return "MUL";
            case Instruction::DIV: return "DIV";
            case Instruction::MOD: return "MOD";
            case Instruction::NEG: return "NEG";
            case Instruction::NOT: return "NOT";
            case Instruction::EQ: return "EQ";
            case Instruction::LT: return "LT";
            case Instruction::LE: return "LE";
            case Instruction::GT: return "GT";
            case Instruction::GE: return "GE";
            case Instruction::JUMP: return "JUMP";
            case Instruction::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
            case Instruction::CALL: return "CALL";
            case Instruction::RETURN: return "RETURN";
            case Instruction::PRINT: return "PRINT";
            case Instruction::HALT: return "HALT";
            default: return "UNKNOWN";
        }
    }
    
    static bool needsOperand(Instruction::Op op) {
        switch (op) {
            case Instruction::PUSH_CONST:
            case Instruction::LOAD_LOCAL:
            case Instruction::STORE_LOCAL:
            case Instruction::LOAD_GLOBAL:
            case Instruction::STORE_GLOBAL:
            case Instruction::JUMP:
            case Instruction::JUMP_IF_FALSE:
                return true;
            default:
                return false;
        }
    }
};

// ============================================================================
// Execution Tracer
// ============================================================================

class ExecutionTracer {
    bool enabled;
    int instructionCount;
    std::ostream* out;
    
public:
    explicit ExecutionTracer(bool e = false, std::ostream& o = std::cout)
        : enabled(e), instructionCount(0), out(&o) {}
    
    void enable() { enabled = true; }
    void disable() { enabled = false; }
    
    void traceInstruction(int pc, const Instruction& inst, 
                         const std::vector<std::string>& constants) {
        if (!enabled) return;
        
        *out << "[" << std::setw(6) << instructionCount++ << "] ";
        *out << "PC=" << std::setw(4) << pc << " ";
        
        BytecodeDisassembler::disassemble(
            Bytecode{inst}, constants, *out);
    }
    
    void traceState(const Value* stack, int stackTop, int frameCount) {
        if (!enabled) return;
        
        *out << "  Stack: [";
        for (int i = 0; i < stackTop && i < 5; i++) {
            if (i > 0) *out << ", ";
            *out << stack[i].toString();
        }
        if (stackTop > 5) *out << ", ...";
        *out << "] (depth=" << stackTop << ")\n";
        *out << "  Frames: " << frameCount << "\n";
    }
    
    void traceResult(const Value& result) {
        if (!enabled) return;
        *out << "\n=== RESULT ===\n";
        *out << result.toString() << "\n\n";
    }
};

// ============================================================================
// Pipeline Visualizer
// ============================================================================

class PipelineVisualizer {
public:
    static void showPipeline(const std::string& source,
                              const ScopeCodeGen::CompileResult& compileResult,
                              std::ostream& out = std::cout) {
        out << "\n========== COMPILATION PIPELINE ==========\n\n";
        
        // Stage 1: Source
        out << "=== SOURCE ===\n";
        out << source << "\n\n";
        
        // Stage 2: Compilation result
        if (!compileResult.success) {
            out << "=== COMPILATION FAILED ===\n";
            for (const auto& e : compileResult.errors) {
                out << "ERROR: " << e << "\n";
            }
            for (const auto& w : compileResult.warnings) {
                out << "WARNING: " << w << "\n";
            }
            return;
        }
        
        // Stage 3: Bytecode
        BytecodeDisassembler::disassemble(compileResult.code, 
                                           compileResult.constants, out);
        
        // Stage 4: Statistics
        out << "=== STATISTICS ===\n";
        out << "Max locals used: " << compileResult.maxLocals << "\n";
        if (!compileResult.warnings.empty()) {
            out << "Warnings: " << compileResult.warnings.size() << "\n";
            for (const auto& w : compileResult.warnings) {
                out << "  - " << w << "\n";
            }
        }
        
        out << "\n";
    }
};

// ============================================================================
// CLI-style interface
// ============================================================================

class KernDiagnostics {
public:
    struct Options {
        bool dumpIR = false;
        bool dumpBytecode = false;
        bool traceExecution = false;
        bool verify = true;
        bool optimize = true;
        int maxInstructions = 1000000;
    };
    
    static int runWithDiagnostics(const std::string& source, const Options& opts) {
        std::cout << "Kern Diagnostics v2.0.2\n\n";
        
        // Compile
        ScopeCodeGen codegen;
        auto result = codegen.compile(source);
        
        PipelineVisualizer::showPipeline(source, result);
        
        if (!result.success) {
            return 1;
        }
        
        // Verify if requested
        if (opts.verify) {
            ComprehensiveVerifier verifier;
            if (!verifier.verify(result.code, result.constants)) {
                std::cout << "VERIFICATION FAILED\n";
                return 1;
            }
            std::cout << "✓ Bytecode verified\n\n";
        }
        
        // Execute with limits
        LimitedVM vm({
            .maxInstructions = (uint64_t)opts.maxInstructions,
            .maxTimeMs = 30000
        });
        
        auto execResult = vm.executeLimited(result.code, result.constants);
        
        if (execResult.ok()) {
            std::cout << "=== OUTPUT ===\n";
            std::cout << execResult.value().toString() << "\n";
            std::cout << "\n=== EXECUTION STATS ===\n";
            std::cout << "Instructions executed: " << vm.getInstructionsExecuted() << "\n";
            return 0;
        } else {
            std::cout << "EXECUTION ERROR: " << execResult.error().message << "\n";
            return 1;
        }
    }
};

} // namespace diagnostics
} // namespace kern
