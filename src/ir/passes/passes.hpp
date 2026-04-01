#ifndef KERN_IR_PASSES_PASSES_HPP
#define KERN_IR_PASSES_PASSES_HPP

#include "ir/ir.hpp"

namespace kern {

void runConstantFolding(IRProgram& program);
void runDeadCodeElimination(IRProgram& program);
void runBasicInlining(IRProgram& program);
void runTypedIRCanonicalization(IRProgram& program);
void runTypedIRConstantPropagation(IRProgram& program);
void runTypedIRDeadBlockElimination(IRProgram& program);

} // namespace kern

#endif // kERN_IR_PASSES_PASSES_HPP
