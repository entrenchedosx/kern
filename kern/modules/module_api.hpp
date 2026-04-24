/* *
 * kern/modules/module_api.hpp - Clean Module API
 * 
 * This defines the only interface modules can use to interact with the VM.
 * No direct VM internals access allowed.
 */
#pragma once

#include "core/value.hpp"
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

namespace kern {

// Module capability flags
enum class ModuleCapability : uint32_t {
    NONE = 0,
    FILE_READ = 1 << 0,
    FILE_WRITE = 1 << 1,
    NETWORK = 1 << 2,
    GRAPHICS = 1 << 3,
    FFI = 1 << 4,
    SHELL = 1 << 5,
    ALL = 0xFFFFFFFF
};

inline ModuleCapability operator|(ModuleCapability a, ModuleCapability b) {
    return static_cast<ModuleCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

inline bool hasCapability(ModuleCapability caps, ModuleCapability check) {
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(check)) != 0;
}

// Graphics context (opaque handle)
class GraphicsContext;

// FFI context (opaque handle)
class FFIContext;

// File system context (opaque handle)
class FileContext;

// Main module interface - passed to all module functions
class ModuleInterface {
public:
    virtual ~ModuleInterface() = default;
    
    // === Value Operations ===
    virtual Value makeNil() = 0;
    virtual Value makeBool(bool val) = 0;
    virtual Value makeInt(int64_t val) = 0;
    virtual Value makeFloat(double val) = 0;
    virtual Value makeString(const std::string& val) = 0;
    virtual Value makeArray() = 0;
    virtual Value makeMap() = 0;
    virtual Value makeError(uint32_t code, const std::string& message) = 0;
    
    virtual bool isTruthy(const Value& val) = 0;
    virtual std::string valueToString(const Value& val) = 0;
    virtual ValueType valueType(const Value& val) = 0;
    
    // Array operations
    virtual void arrayPush(Value& arr, const Value& elem) = 0;
    virtual Value arrayGet(const Value& arr, size_t idx) = 0;
    virtual size_t arraySize(const Value& arr) = 0;
    
    // Map operations
    virtual void mapSet(Value& map, const std::string& key, const Value& val) = 0;
    virtual Value mapGet(const Value& map, const std::string& key) = 0;
    virtual bool mapHas(const Value& map, const std::string& key) = 0;
    virtual size_t mapSize(const Value& map) = 0;
    
    // === Error Handling ===
    virtual void raiseError(const std::string& message) = 0;
    virtual void raiseError(uint32_t code, const std::string& message) = 0;
    virtual bool hasError() = 0;
    virtual std::string getError() = 0;
    virtual void clearError() = 0;
    
    // === Logging ===
    virtual void logInfo(const std::string& msg) = 0;
    virtual void logWarning(const std::string& msg) = 0;
    virtual void logError(const std::string& msg) = 0;
    
    // === Capability-gated Operations ===
    virtual bool checkCapability(ModuleCapability cap) = 0;
    
    // File operations (requires FILE_READ/FILE_WRITE)
    virtual FileContext* getFileContext() = 0;
    
    // Graphics operations (requires GRAPHICS)
    virtual GraphicsContext* getGraphicsContext() = 0;
    
    // FFI operations (requires FFI)
    virtual FFIContext* getFFIContext() = 0;
    
    // === Stack Trace ===
    virtual std::vector<std::string> getStackTrace() = 0;
};

// File context interface
class FileContext {
public:
    virtual ~FileContext() = default;
    
    virtual Result<std::string> readFile(const std::string& path) = 0;
    virtual Result<void> writeFile(const std::string& path, const std::string& content) = 0;
    virtual Result<bool> exists(const std::string& path) = 0;
    virtual Result<std::vector<std::string>> listDirectory(const std::string& path) = 0;
    virtual Result<void> createDirectory(const std::string& path) = 0;
};

// Graphics context interface
class GraphicsContext {
public:
    virtual ~GraphicsContext() = default;
    
    // Window
    virtual Result<void> createWindow(int width, int height, const std::string& title) = 0;
    virtual void closeWindow() = 0;
    virtual bool windowShouldClose() = 0;
    
    // 2D Drawing
    virtual void clear(uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void drawLine(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, bool fill) = 0;
    virtual void drawCircle(int x, int y, int radius, uint8_t r, uint8_t g, uint8_t b, bool fill) = 0;
    virtual void drawText(int x, int y, const std::string& text, uint8_t r, uint8_t g, uint8_t b) = 0;
    
    // 3D Drawing
    virtual void begin3D() = 0;
    virtual void end3D() = 0;
    virtual int createCube(float size) = 0;  // Returns object ID
    virtual int createSphere(float radius) = 0;
    virtual void setObjectPosition(int id, float x, float y, float z) = 0;
    virtual void setObjectColor(int id, uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void drawObject(int id) = 0;
    
    // Frame management
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
};

// FFI context interface
class FFIContext {
public:
    virtual ~FFIContext() = default;
    
    virtual Result<void*> loadLibrary(const std::string& path) = 0;
    virtual Result<void*> getSymbol(void* lib, const std::string& name) = 0;
    virtual Result<Value> callFunction(void* func, const std::vector<Value>& args) = 0;
    virtual void freeLibrary(void* lib) = 0;
};

// Module registration
using ModuleInitFn = std::function<void(ModuleInterface* iface, std::unordered_map<std::string, Value>& exports)>;

struct ModuleInfo {
    std::string name;
    std::string version;
    std::string description;
    ModuleCapability requiredCapabilities;
    ModuleInitFn init;
};

// Module registry
class ModuleRegistry {
    std::unordered_map<std::string, ModuleInfo> modules;
    
public:
    void registerModule(const ModuleInfo& info);
    bool hasModule(const std::string& name) const;
    const ModuleInfo* getModule(const std::string& name) const;
    std::vector<std::string> listModules() const;
};

} // namespace kern
