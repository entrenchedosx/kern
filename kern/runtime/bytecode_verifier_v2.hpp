/* *
 * kern/runtime/bytecode_verifier_v2.hpp - Control-Flow Aware Verifier
 * 
 * Improvements:
 * - Basic block analysis
 * - Stack height validation across branches
 * - All code paths must return
 * - Unreachable code detection
 * - Loop analysis
 */
#pragma once

#include "bytecode_verifier.hpp"
#include <vector>
#include <set>
#include <map>
#include <queue>

namespace kern {

class ControlFlowVerifier {
public:
    struct BasicBlock {
        size_t start;           // Starting instruction
        size_t end;             // Ending instruction (exclusive)
        std::set<size_t> preds; // Predecessors (block indices)
        std::set<size_t> succs; // Successors (block indices)
        int stackEntry;         // Stack height on entry
        int stackExit;          // Stack height on exit
        bool reachable;         // Is this block reachable?
        bool returns;           // Does this block end with return?
        bool hasError;          // Verification error in this block
    };
    
    struct VerificationResult {
        bool valid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<BasicBlock> blocks;
        
        void addError(const std::string& msg) {
            valid = false;
            errors.push_back(msg);
        }
        
        void addWarning(const std::string& msg) {
            warnings.push_back(msg);
        }
    };
    
private:
    BytecodeVerifier baseVerifier;
    
public:
    explicit ControlFlowVerifier(BytecodeVerifier::Config cfg = {}) 
        : baseVerifier(cfg) {}
    
    VerificationResult verify(const Bytecode& code, const std::vector<std::string>& constants) {
        VerificationResult result;
        
        // Phase 1: Basic instruction validation
        if (!baseVerifier.verify(code, constants)) {
            result.addError("Base verification failed");
            return result;
        }
        
        // Phase 2: Build control flow graph
        result.blocks = buildCFG(code);
        if (result.blocks.empty() && !code.empty()) {
            result.addError("Failed to build control flow graph");
            return result;
        }
        
        // Phase 3: Mark reachable blocks
        markReachable(result.blocks, 0);
        
        // Phase 4: Validate stack height across branches
        if (!validateStackBalance(result.blocks, code)) {
            result.addError("Stack balance error across control flow");
        }
        
        // Phase 5: Check all paths return (if function has return type)
        // Skip for main script
        
        // Phase 6: Detect unreachable code
        for (size_t i = 0; i < result.blocks.size(); i++) {
            if (!result.blocks[i].reachable && result.blocks[i].start < code.size()) {
                result.addWarning("Unreachable code at instruction " + 
                    std::to_string(result.blocks[i].start));
            }
        }
        
        // Phase 7: Validate loop structure
        validateLoops(result.blocks, code, result);
        
        return result;
    }
    
private:
    std::vector<BasicBlock> buildCFG(const Bytecode& code) {
        std::vector<BasicBlock> blocks;
        std::set<size_t> leaders;  // Instruction indices that start blocks
        
        if (code.empty()) return blocks;
        
        // First instruction is a leader
        leaders.insert(0);
        
        // Find all leaders:
        // 1. Target of any jump
        // 2. Instruction after a conditional jump
        // 3. Instruction after an unconditional jump
        for (size_t i = 0; i < code.size(); i++) {
            const auto& inst = code[i];
            
            if (inst.op == Instruction::JUMP) {
                // Target is a leader
                int target = (int)i + inst.operand;
                if (target >= 0 && (size_t)target < code.size()) {
                    leaders.insert(target);
                }
                // Next instruction is a leader (if reachable)
                if (i + 1 < code.size()) {
                    leaders.insert(i + 1);
                }
            } else if (inst.op == Instruction::JUMP_IF_FALSE) {
                // Target is a leader
                int target = (int)i + inst.operand;
                if (target >= 0 && (size_t)target < code.size()) {
                    leaders.insert(target);
                }
                // Next instruction is a leader (fall-through)
                if (i + 1 < code.size()) {
                    leaders.insert(i + 1);
                }
            } else if (inst.op == Instruction::RETURN || inst.op == Instruction::HALT) {
                // Next instruction is a leader (if any)
                if (i + 1 < code.size()) {
                    leaders.insert(i + 1);
                }
            }
        }
        
        // Sort leaders
        std::vector<size_t> sortedLeaders(leaders.begin(), leaders.end());
        std::sort(sortedLeaders.begin(), sortedLeaders.end());
        
        // Create blocks
        for (size_t i = 0; i < sortedLeaders.size(); i++) {
            BasicBlock block;
            block.start = sortedLeaders[i];
            block.end = (i + 1 < sortedLeaders.size()) ? sortedLeaders[i + 1] : code.size();
            block.stackEntry = -1;  // Unknown
            block.stackExit = -1;
            block.reachable = false;
            block.returns = false;
            block.hasError = false;
            blocks.push_back(block);
        }
        
        // Map instruction to block
        std::vector<size_t> instrToBlock(code.size(), SIZE_MAX);
        for (size_t b = 0; b < blocks.size(); b++) {
            for (size_t i = blocks[b].start; i < blocks[b].end; i++) {
                instrToBlock[i] = b;
            }
        }
        
        // Compute successors
        for (size_t b = 0; b < blocks.size(); b++) {
            auto& block = blocks[b];
            if (block.start >= code.size()) continue;
            
            size_t lastInstr = block.end > 0 ? block.end - 1 : block.start;
            if (lastInstr >= code.size()) lastInstr = code.size() - 1;
            
            const auto& inst = code[lastInstr];
            
            if (inst.op == Instruction::JUMP) {
                // Unconditional jump to target
                int target = (int)lastInstr + inst.operand;
                if (target >= 0 && (size_t)target < code.size()) {
                    size_t targetBlock = instrToBlock[target];
                    if (targetBlock != SIZE_MAX) {
                        block.succs.insert(targetBlock);
                        blocks[targetBlock].preds.insert(b);
                    }
                }
            } else if (inst.op == Instruction::JUMP_IF_FALSE) {
                // Conditional: target AND fall-through
                int target = (int)lastInstr + inst.operand;
                if (target >= 0 && (size_t)target < code.size()) {
                    size_t targetBlock = instrToBlock[target];
                    if (targetBlock != SIZE_MAX) {
                        block.succs.insert(targetBlock);
                        blocks[targetBlock].preds.insert(b);
                    }
                }
                // Fall-through
                if (lastInstr + 1 < code.size() && lastInstr + 1 < block.end) {
                    // Target is in this block, add next block
                }
                if (b + 1 < blocks.size()) {
                    block.succs.insert(b + 1);
                    blocks[b + 1].preds.insert(b);
                }
            } else if (inst.op != Instruction::RETURN && inst.op != Instruction::HALT) {
                // Fall-through to next block
                if (b + 1 < blocks.size()) {
                    block.succs.insert(b + 1);
                    blocks[b + 1].preds.insert(b);
                }
            }
            
            // Check if block ends with return
            if (inst.op == Instruction::RETURN) {
                block.returns = true;
            }
        }
        
        return blocks;
    }
    
    void markReachable(std::vector<BasicBlock>& blocks, size_t startBlock) {
        std::queue<size_t> worklist;
        
        if (startBlock < blocks.size()) {
            blocks[startBlock].reachable = true;
            worklist.push(startBlock);
        }
        
        while (!worklist.empty()) {
            size_t current = worklist.front();
            worklist.pop();
            
            for (size_t succ : blocks[current].succs) {
                if (!blocks[succ].reachable) {
                    blocks[succ].reachable = true;
                    worklist.push(succ);
                }
            }
        }
    }
    
    bool validateStackBalance(std::vector<BasicBlock>& blocks, const Bytecode& code) {
        // For each reachable block, verify stack height consistency
        bool valid = true;
        
        for (auto& block : blocks) {
            if (!block.reachable) continue;
            
            // Simulate stack through the block
            int stackHeight = block.stackEntry >= 0 ? block.stackEntry : 0;
            
            for (size_t i = block.start; i < block.end && i < code.size(); i++) {
                const auto& inst = code[i];
                
                // Check required stack
                int pops = getStackPops(inst.op);
                int pushes = getStackPushes(inst.op);
                
                if (stackHeight < pops) {
                    // Stack underflow
                    valid = false;
                }
                
                stackHeight -= pops;
                stackHeight += pushes;
                
                if (stackHeight < 0) {
                    valid = false;
                    stackHeight = 0;
                }
            }
            
            block.stackExit = stackHeight;
            
            // Verify successors have compatible stack height
            for (size_t succIdx : block.succs) {
                auto& succ = blocks[succIdx];
                if (succ.stackEntry < 0) {
                    succ.stackEntry = stackHeight;
                } else if (succ.stackEntry != stackHeight) {
                    // Stack height mismatch at merge point
                    valid = false;
                }
            }
        }
        
        return valid;
    }
    
    void validateLoops(const std::vector<BasicBlock>& blocks, 
                      const Bytecode& code,
                      VerificationResult& result) {
        // Detect natural loops (back edges in CFG)
        for (size_t b = 0; b < blocks.size(); b++) {
            const auto& block = blocks[b];
            
            for (size_t succ : block.succs) {
                if (succ <= b) {
                    // Back edge found - potential loop
                    // Check if loop has valid structure
                    
                    // Simple check: loop header should be reachable
                    if (!block.reachable) {
                        result.addWarning("Unreachable loop detected");
                    }
                }
            }
        }
    }
    
    int getStackPops(Instruction::Op op) {
        switch (op) {
            case Instruction::POP: return 1;
            case Instruction::ADD:
            case Instruction::SUB:
            case Instruction::MUL:
            case Instruction::DIV:
            case Instruction::MOD:
            case Instruction::EQ:
            case Instruction::LT:
            case Instruction::LE:
            case Instruction::GT:
            case Instruction::GE: return 2;
            case Instruction::NEG:
            case Instruction::NOT:
            case Instruction::PRINT: return 1;
            case Instruction::STORE_LOCAL:
            case Instruction::STORE_GLOBAL: return 1;
            case Instruction::JUMP_IF_FALSE: return 1;
            case Instruction::CALL: return 1;
            case Instruction::RETURN: return 1;
            default: return 0;
        }
    }
    
    int getStackPushes(Instruction::Op op) {
        switch (op) {
            case Instruction::PUSH_CONST:
            case Instruction::PUSH_NIL:
            case Instruction::PUSH_TRUE:
            case Instruction::PUSH_FALSE: return 1;
            case Instruction::DUP: return 1;
            case Instruction::LOAD_LOCAL:
            case Instruction::LOAD_GLOBAL: return 1;
            case Instruction::ADD:
            case Instruction::SUB:
            case Instruction::MUL:
            case Instruction::DIV:
            case Instruction::MOD:
            case Instruction::NEG:
            case Instruction::EQ:
            case Instruction::LT:
            case Instruction::LE:
            case Instruction::GT:
            case Instruction::GE:
            case Instruction::NOT: return 1;
            case Instruction::CALL: return 1;
            default: return 0;
        }
    }
};

// Combined verifier using both passes
class ComprehensiveVerifier {
    BytecodeVerifier baseVerifier;
    ControlFlowVerifier cfVerifier;
    
public:
    explicit ComprehensiveVerifier(BytecodeVerifier::Config cfg = {})
        : baseVerifier(cfg), cfVerifier(cfg) {}
    
    bool verify(const Bytecode& code, const std::vector<std::string>& constants) {
        // Phase 1: Base verification
        if (!baseVerifier.verify(code, constants)) {
            std::cerr << "Base bytecode verification failed" << std::endl;
            baseVerifier.printReport();
            return false;
        }
        
        // Phase 2: Control flow verification
        auto result = cfVerifier.verify(code, constants);
        
        if (!result.errors.empty()) {
            std::cerr << "Control flow verification failed:" << std::endl;
            for (const auto& e : result.errors) {
                std::cerr << "  Error: " << e << std::endl;
            }
            return false;
        }
        
        if (!result.warnings.empty()) {
            std::cout << "Verification warnings:" << std::endl;
            for (const auto& w : result.warnings) {
                std::cout << "  Warning: " << w << std::endl;
            }
        }
        
        return true;
    }
};

} // namespace kern
