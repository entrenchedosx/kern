/* *
 * kern Game builtins - Window, rendering, input, timing, sound, text (Raylib backend).
 * graphics/game are exposed as a module (Python-style): import game; game.window_create(...)
 * use createGameModule() for __import("game"). Use registerGameBuiltins() only for kern_game (globals).
 */
#ifndef KERN_GAME_BUILTINS_HPP
#define KERN_GAME_BUILTINS_HPP

#include <vector>
#include <string>
#include <memory>
#include "vm/value.hpp"

namespace kern {
class VM;

/* * register game builtins as globals (for kern_game runner).*/
void registerGameBuiltins(VM& vm);
/* * create the game module (map of name -> function). Use for import game.*/
ValuePtr createGameModule(VM& vm);
std::vector<std::string> getGameBuiltinNames();

/* * shared graphics window (used by game and 2dgraphics).*/
bool graphicsWindowOpen();
void graphicsInitWindow(int width, int height, const std::string& title);
void graphicsCloseWindow();
/* * begin/end a drawing frame (for 2dgraphics manual frame).*/
void graphicsBeginFrame();
void graphicsEndFrame();
/* * camera (used by 2dgraphics and game).*/
void graphicsSetCameraTarget(float x, float y);
void graphicsMoveCameraTarget(float dx, float dy);
void graphicsSetCameraZoom(float zoom);
void graphicsSetCameraEnabled(bool enable);
void graphicsBegin2D();
void graphicsEnd2D();
/* * window (for 2dgraphics).*/
void graphicsSetWindowSize(int width, int height);
void graphicsToggleFullscreen();
} // namespace kern

#endif
