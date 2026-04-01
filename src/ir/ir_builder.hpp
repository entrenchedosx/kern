#ifndef KERN_IR_IR_BUILDER_HPP
#define KERN_IR_IR_BUILDER_HPP

#include "compiler/project_resolver.hpp"
#include "ir/ir.hpp"

namespace kern {

IRProgram buildIRFromResolvedGraph(const ResolveResult& resolved);

} // namespace kern

#endif // kERN_IR_IR_BUILDER_HPP
