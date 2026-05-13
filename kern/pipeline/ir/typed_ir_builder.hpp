#ifndef KERN_IR_TYPED_IR_BUILDER_HPP
#define KERN_IR_TYPED_IR_BUILDER_HPP

#include "ir/ir.hpp"

namespace kern {

TypedIRModule buildTypedIRModule(const std::string& modulePath, const std::string& source);
void buildTypedIRForProgram(IRProgram& program);

} // namespace kern

#endif // kERN_IR_TYPED_IR_BUILDER_HPP
