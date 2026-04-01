/* *
 * kern g3d module – basic 3D graphics (window, camera, simple shapes).
 * use: let g3 = import("g3d"); g3.createWindow(800, 600, "Title"); g3.run(update, draw);
 */
#ifndef KERN_G3D_H
#define KERN_G3D_H

#include "vm/value.hpp"
#include <memory>

namespace kern {
class VM;

/* * create the g3d module (map of name -> function). Used by import("g3d").*/
ValuePtr create3dGraphicsModule(VM& vm);
} // namespace kern

#endif

