/* *
 * kern g2d module – 2D graphics (window, render loop, shapes, text, colors).
 * use: let g = import("g2d"); g.createWindow(800, 600, "Title"); g.run(update, draw);
 */
#ifndef KERN_G2D_H
#define KERN_G2D_H

#include "bytecode/value.hpp"
#include <memory>

namespace kern {
class VM;

/* * create the g2d module (map of name -> function). Used by import("g2d").*/
ValuePtr create2dGraphicsModule(VM& vm);
} // namespace kern

#endif
