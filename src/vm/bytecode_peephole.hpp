/* *
 * Safe bytecode peephole: remove NOPs that are not control-flow targets, with operand remapping.
 */

#ifndef KERN_BYTECODE_PEEPHOLE_HPP
#define KERN_BYTECODE_PEEPHOLE_HPP

#include "bytecode.hpp"

namespace kern {

void applyBytecodePeephole(Bytecode& code);

} // namespace kern

#endif
