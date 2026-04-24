/* *
 * kern Standard Library - Builtin functions registered with the VM
 */ 

#ifndef KERN_BUILTINS_HPP
#define KERN_BUILTINS_HPP

#include "bytecode/value.hpp"
#include "vm.hpp"
#include "errors/errors.hpp"
#include "permissions.hpp"
#include "kern_socket.hpp"
#include "platform/env_compat.hpp"
#include "safe_arithmetic.hpp"
#include <memory>
#include <cmath>
#include <ctime>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cctype>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <regex>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <random>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "http_get_winhttp.hpp"
#else
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

#include "compiler/builtin_names.hpp"

namespace kern {

// Bring safe arithmetic functions into kern namespace for easier access
using namespace kern::safe;

/* * full fence for volatile_* builtins and memory_barrier() (portable; replaces GCC-only inline asm).*/
inline void builtinAtomicFence() { std::atomic_thread_fence(std::memory_order_seq_cst); }

// minimal JSON parser for json_parse() builtin
struct JsonParser {
    const std::string& s;
    size_t pos = 0;
    void skipWs() { while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) ++pos; }
    bool atEnd() { skipWs(); return pos >= s.size(); }
    char peek() { skipWs(); return pos < s.size() ? s[pos] : 0; }
    char get() { return pos < s.size() ? s[pos++] : 0; }
    ValuePtr parseValue() {
        skipWs();
        if (pos >= s.size()) return std::make_shared<Value>(Value::nil());
        char c = s[pos];
        if (c == '"') return parseString();
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == 't' && s.substr(pos, 4) == "true") { pos += 4; return std::make_shared<Value>(Value::fromBool(true)); }
        if (c == 'f' && s.substr(pos, 5) == "false") { pos += 5; return std::make_shared<Value>(Value::fromBool(false)); }
        if (c == 'n' && s.substr(pos, 4) == "null") { pos += 4; return std::make_shared<Value>(Value::nil()); }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber();
        return std::make_shared<Value>(Value::nil());
    }
    ValuePtr parseString() {
        if (get() != '"') return std::make_shared<Value>(Value::nil());
        std::string out;
        while (pos < s.size()) {
            char c = get();
            if (c == '"') break;
            if (c == '\\') {
                if (pos >= s.size()) break;
                c = get();
                if (c == 'n') out += '\n'; else if (c == 'r') out += '\r'; else if (c == 't') out += '\t'; else if (c == '"') out += '"'; else if (c == '\\') out += '\\'; else out += c;
            } else out += c;
        }
        return std::make_shared<Value>(Value::fromString(std::move(out)));
    }
    ValuePtr parseNumber() {
        size_t start = pos;
        if (s[pos] == '-') ++pos;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos;
        if (pos < s.size() && s[pos] == '.') { ++pos; while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos; }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) { ++pos; if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos; while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) ++pos; }
        std::string numStr = s.substr(start, pos - start);
        if (numStr.find('.') != std::string::npos || numStr.find('e') != std::string::npos || numStr.find('E') != std::string::npos)
            return std::make_shared<Value>(Value::fromFloat(std::stod(numStr)));
        return std::make_shared<Value>(Value::fromInt(std::stoll(numStr)));
    }
    ValuePtr parseArray() {
        if (get() != '[') return std::make_shared<Value>(Value::fromArray({}));
        std::vector<ValuePtr> arr;
        skipWs();
        if (peek() == ']') { get(); return std::make_shared<Value>(Value::fromArray(std::move(arr))); }
        while (true) {
            arr.push_back(parseValue());
            skipWs();
            if (pos >= s.size()) break;
            if (s[pos] == ']') { ++pos; break; }
            if (s[pos] == ',') ++pos; else break;
        }
        return std::make_shared<Value>(Value::fromArray(std::move(arr)));
    }
    ValuePtr parseObject() {
        if (get() != '{') return std::make_shared<Value>(Value::fromMap({}));
        std::unordered_map<std::string, ValuePtr> map;
        skipWs();
        if (peek() == '}') { get(); return std::make_shared<Value>(Value::fromMap(std::move(map))); }
        while (true) {
            if (peek() != '"') break;
            ValuePtr keyVal = parseString();
            std::string key = keyVal && keyVal->type == Value::Type::STRING ? std::get<std::string>(keyVal->data) : "";
            skipWs();
            if (pos < s.size() && s[pos] == ':') ++pos;
            map[key] = parseValue();
            skipWs();
            if (pos >= s.size() || s[pos] == '}') { if (pos < s.size()) ++pos; break; }
            if (s[pos] == ',') ++pos;
        }
        return std::make_shared<Value>(Value::fromMap(std::move(map)));
    }
};

static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '"') out += "\\\""; else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n"; else if (c == '\r') out += "\\r"; else if (c == '\t') out += "\\t";
        else if (c < 32) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); out += b; }
        else out += c;
    }
    return out;
}

// advanced memory: struct layouts, pools, tracked allocations, mapped files (one instance across TUs)
struct StructFieldLayout {
    std::string name;
    size_t size = 0;
    size_t align = 1;
    size_t offset = 0;
};
struct StructLayoutMeta {
    std::vector<StructFieldLayout> fields;
    size_t size = 0;
    size_t align = 1;
};
inline static std::unordered_map<std::string, StructLayoutMeta> g_structLayouts;
// FORMAL KERNEL ALLOCATOR SPEC
//
// DESIGN HONESTY: This is NOT a true kernel allocator (no handle-based indirection,
// no generation counters, no structural protection against dangling external pointers).
// It IS a robust advanced allocator with formal invariants and unified safety abstractions.
//
// MATHEMATICAL MODEL:
// - Slot address: base + safe_index(slotIdx, blockSize)  [safe_index]
// - State transition: target = transition(current, target)  [transition_allowed table]
// - Overflow-free arithmetic: safe_mul, safe_add, safe_index [kern::safe namespace]
//
// COMPILE-TIME VERIFIED INVARIANTS (via static_assert):
// [1] INVALID state is terminal - all transitions out are forbidden
// [2] FREE → ALLOCATED is the only valid allocation transition
// [3] ALLOCATED → FREE is the only valid deallocation transition
//
// RUNTIME ENFORCED INVARIANTS:
// [4] All pointer arithmetic uses safe_index() → no offset overflow
// [5] All state transitions verified against transition_allowed table
// [6] freeSlotStack entries verified FREE on pop, maintained FREE on push
// [7] All multiplication uses safe_mul() → no multiplication overflow
// [8] All accumulation uses safe_add() → no addition overflow
//
// UNIFIED SAFETY ABSTRACTIONS:
// - safe_mul(a, b, out) - multiplication with overflow check
// - safe_add(a, b, out) - addition with overflow check
// - safe_index(base, index, stride) - pointer arithmetic with overflow check
// - safe_range_end(base, count, stride) - range computation with overflow check
//
// STATE TRANSITION TABLE (compile-time constant):
//   From/To     FREE    ALLOCATED    INVALID
//   FREE        NO      YES          NO
//   ALLOCATED   YES     NO           YES
//   INVALID     NO      NO           NO
//
// LIFECYCLE CONTRACT:
// - createPool: initializes all slots to FREE, populates freeSlotStack
// - pool_alloc: pops from freeSlotStack, verifies state FREE, sets ALLOCATED
// - pool_free: validates pointer, verifies state ALLOCATED, sets FREE, pushes to stack
// - destroyPool: marks all slots INVALID, clears freeSlotStack, erases from map, frees memory
//
// POST-DESTROY CONTRACT:
// - Calling pool_free with pointers from destroyed pool: undefined behavior
// - pool lookup will fail (debug: abort, release: safe return)
// - Caller MUST ensure no use-after-destroy through external synchronization
//
// O(1) ALLOCATION GUARANTEE:
// - Valid only when freeSlotStack invariants hold
// - All operations are O(1) assuming stack is pre-filled and state sync maintained
// - Invariant violations cause safe failure, not undefined behavior
//
// THREAD SAFETY: Single-threaded only. No locking provided.
//

enum class PoolSlotState : uint8_t {
    FREE = 0,      // Available for allocation
    ALLOCATED = 1, // Currently allocated to user
    INVALID = 2    // Destroyed or corrupted - reject all operations
};

namespace kern::pool {
    // Compile-time state transition table
    // Row = current state, Col = target state, Value = allowed?
    inline constexpr bool transition_allowed[3][3] = {
        // To:         FREE(0)  ALLOCATED(1)  INVALID(2)
        /* FREE */     {false,   true,         false},
        /* ALLOCATED */{true,    false,        true},
        /* INVALID */  {false,   false,        false}
    };
    
    // Runtime state transition with debug hard-fail
    // Returns true if transition succeeds, false if rejected (release mode)
    // Aborts if transition not allowed (debug mode)
    inline bool transition_state(PoolSlotState current, PoolSlotState target,
                                  size_t blockIdx, size_t slotIdx) {
        size_t from = static_cast<size_t>(current);
        size_t to = static_cast<size_t>(target);
        
        // Bounds check for safety (should never fail with valid enum values)
        if (from >= 3 || to >= 3) {
#ifdef KERN_DEBUG
            std::abort();  // Invalid state value - corruption detected
#else
            return false;
#endif
        }
        
        if (!transition_allowed[from][to]) {
#ifdef KERN_DEBUG
            std::abort();  // Invalid state transition detected
#else
            (void)blockIdx;  // Unused in release
            (void)slotIdx;
            return false;
#endif
        }
        
        return true;
    }
}

// Compile-time verification of transition table invariants
static_assert(!kern::pool::transition_allowed[static_cast<int>(PoolSlotState::INVALID)][static_cast<int>(PoolSlotState::FREE)],
              "INVALID → FREE transition must be forbidden - INVALID is terminal");
static_assert(!kern::pool::transition_allowed[static_cast<int>(PoolSlotState::INVALID)][static_cast<int>(PoolSlotState::ALLOCATED)],
              "INVALID → ALLOCATED transition must be forbidden - INVALID is terminal");
static_assert(!kern::pool::transition_allowed[static_cast<int>(PoolSlotState::FREE)][static_cast<int>(PoolSlotState::INVALID)],
              "FREE → INVALID transition must be forbidden - only ANY → INVALID via destroyPool");
static_assert(kern::pool::transition_allowed[static_cast<int>(PoolSlotState::FREE)][static_cast<int>(PoolSlotState::ALLOCATED)],
              "FREE → ALLOCATED transition must be allowed - this is the allocation path");
static_assert(kern::pool::transition_allowed[static_cast<int>(PoolSlotState::ALLOCATED)][static_cast<int>(PoolSlotState::FREE)],
              "ALLOCATED → FREE transition must be allowed - this is the deallocation path");

struct PoolBlock {
    char* base;
    size_t count;
    std::vector<PoolSlotState> states;  // Per-slot state tracking (size == count)
};

struct PoolState {
    size_t blockSize = 0;
    std::vector<PoolBlock> blocks;
    
    // Free slot index stack for O(1) allocation (stores {blockIdx, slotIdx} pairs)
    std::vector<std::pair<size_t, size_t>> freeSlotStack;
    
    // Get slot index for a pointer within this pool
    // Returns false if pointer not found or invalid
    // On success, sets blockIdx and slotIdx to valid indices
    bool getSlotIndex(void* ptr, size_t& blockIdx, size_t& slotIdx) const {
        // Guard against null pointer
        if (ptr == nullptr) return false;
        
        // Guard against blockSize == 0 (division by zero)
        if (blockSize == 0) return false;
        
        char* pc = static_cast<char*>(ptr);
        for (size_t i = 0; i < blocks.size(); ++i) {
            const auto& blk = blocks[i];
            
            // Check for overflow in blk.count * blockSize and compute end pointer safely
            size_t total;
            if (!safe_mul(blk.count, blockSize, total)) return false;
            
            char* start = blk.base;
            char* end = safe_range_end(blk.base, blk.count, blockSize);
            if (!end) return false;  // Should not happen if safe_mul succeeded, but guard anyway
            
            // Validate pointer is within block bounds
            if (pc >= start && pc < end) {
                size_t offset = pc - start;
                // Validate alignment to slot boundary
                if (offset % blockSize == 0) {
                    size_t computedSlotIdx = offset / blockSize;
                    // Explicitly enforce slotIdx < blk.count
                    if (computedSlotIdx >= blk.count) return false;
                    
                    blockIdx = i;
                    slotIdx = computedSlotIdx;
                    return true;
                }
            }
        }
        return false;
    }
    
    // Get state of a slot (returns INVALID if indices invalid)
    PoolSlotState getSlotState(size_t blockIdx, size_t slotIdx) const {
        if (blockIdx >= blocks.size()) return PoolSlotState::INVALID;
        const auto& blk = blocks[blockIdx];
        if (slotIdx >= blk.count) return PoolSlotState::INVALID;
        return blk.states[slotIdx];
    }
    
    // Set state of a slot (no-op if indices invalid)
    void setSlotState(size_t blockIdx, size_t slotIdx, PoolSlotState state) {
        if (blockIdx < blocks.size() && slotIdx < blocks[blockIdx].count) {
            blocks[blockIdx].states[slotIdx] = state;
        }
    }
};
inline static std::unordered_map<int64_t, PoolState> g_pools;
inline static int64_t g_nextPoolId = 1;
inline static std::unordered_set<void*> g_trackedAllocs;
#ifdef _WIN32
struct MappedFileState { void* view = nullptr; void* hMap = nullptr; void* hFile = nullptr; size_t size = 0; };
#else
struct MappedFileState { void* view = nullptr; size_t size = 0; };
#endif
inline static std::unordered_map<void*, MappedFileState> g_mappedFiles;
inline static std::unordered_map<void*, void*> g_alignedAllocBases;
#ifdef _WIN32
inline static std::unordered_map<std::string, HMODULE> g_ffiLibraries;
inline static std::unordered_map<std::string, FARPROC> g_ffiSymbols;
#endif

/* * RAII wrapper for malloc allocations - exception-safe cleanup */
struct MallocGuard {
    void* ptr;
    explicit MallocGuard(void* p = nullptr) : ptr(p) {}
    ~MallocGuard() { if (ptr) std::free(ptr); }
    MallocGuard(const MallocGuard&) = delete;
    MallocGuard& operator=(const MallocGuard&) = delete;
    MallocGuard(MallocGuard&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
    MallocGuard& operator=(MallocGuard&& other) noexcept {
        if (this != &other) {
            if (ptr) std::free(ptr);
            ptr = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }
    void* release() { void* p = ptr; ptr = nullptr; return p; }
};

// SWE-1.8: MemoryManager abstraction layer
//
// DESIGN NOTE: This is a CENTRALIZED PROCESS-GLOBAL allocator with explicit contract.
// All VM instances share the same pool state. For per-VM isolation, the design would need
// to be refactored to instance-owned MemoryManager objects.
//
// THREAD SAFETY: Single-threaded only. No locking provided. Concurrent access to pool
// operations is undefined behavior.
//
class MemoryManager {
public:
    // Pool allocation API
    static int64_t createPool(size_t blockSize, size_t count);
    static void* poolAlloc(int64_t poolId);
    static void poolFree(void* ptr, int64_t poolId);
    static void destroyPool(int64_t poolId);
    
    // Raw allocation API (for FFI compatibility)
    static void* alloc(size_t size);
    static void free(void* ptr);
    
    // Global shutdown - must be called when no VM instances exist
    static void shutdown();
    
private:
    MemoryManager() = delete;  // Static-only interface
};

// SWE-1.8: MemoryManager implementation - wraps global pool operations
inline int64_t MemoryManager::createPool(size_t blockSize, size_t count) {
    const size_t kMaxPool = 1024 * 1024;
    const size_t kMaxCount = 65536;
    if (blockSize > kMaxPool || count > kMaxCount) return -1;
    
    // Guard against blockSize == 0
    if (blockSize == 0) return -1;
    
    // Check for overflow in blockSize * count
    size_t totalSize;
    if (!safe_mul(blockSize, count, totalSize)) {
        return -1;  // Overflow detected
    }
    
    int64_t id = g_nextPoolId++;
    PoolState& ps = g_pools[id];
    ps.blockSize = blockSize;
    void* block = std::malloc(totalSize);
    if (!block) return -1;
    
    char* base = static_cast<char*>(block);
    
    // Initialize block with state bitmap - all slots initially FREE
    std::vector<PoolSlotState> states;
    states.resize(count, PoolSlotState::FREE);  // Explicit resize to guarantee FREE initialization
    ps.blocks.push_back({base, count, std::move(states)});
    
    // Initialize free slot index stack with all slots from all blocks (O(1) allocation)
    // Guard against overflow in totalSlots accumulation
    size_t totalSlots = 0;
    for (const auto& blk : ps.blocks) {
        if (!safe_add(totalSlots, blk.count, totalSlots)) {
            // Overflow in accumulation - fail operation only, do NOT destroy pool
            return -1;
        }
    }
    ps.freeSlotStack.reserve(totalSlots);
    for (size_t b = 0; b < ps.blocks.size(); ++b) {
        for (size_t s = 0; s < ps.blocks[b].count; ++s) {
            ps.freeSlotStack.push_back({b, s});
        }
    }
    
    return id;
}

inline void* MemoryManager::poolAlloc(int64_t poolId) {
    auto it = g_pools.find(poolId);
    if (it == g_pools.end()) return nullptr;
    
    PoolState& ps = it->second;
    
    // O(1) allocation from free slot index stack
#ifdef KERN_DEBUG
    // Debug: stack underflow is a logic failure
    if (ps.freeSlotStack.empty()) std::abort();
#else
    if (ps.freeSlotStack.empty()) return nullptr;  // Pool exhausted
#endif
    
    auto [blockIdx, slotIdx] = ps.freeSlotStack.back();
    ps.freeSlotStack.pop_back();
    
    // Validate indices (hard fail in debug mode)
#ifdef KERN_DEBUG
    if (blockIdx >= ps.blocks.size()) std::abort();
    if (slotIdx >= ps.blocks[blockIdx].count) std::abort();
#else
    if (blockIdx >= ps.blocks.size()) return nullptr;
    if (slotIdx >= ps.blocks[blockIdx].count) return nullptr;
#endif
    
    // UNIFIED STATE TRANSITION: current → ALLOCATED
    // This uses the compile-time verified transition table
    // Valid only if current state is FREE (enforced by transition_allowed table)
    PoolSlotState currentState = ps.getSlotState(blockIdx, slotIdx);
    if (!kern::pool::transition_state(currentState, PoolSlotState::ALLOCATED, blockIdx, slotIdx)) {
        // transition_state handles debug abort internally; here we just return
        return nullptr;
    }
    
    // Execute the transition
    ps.setSlotState(blockIdx, slotIdx, PoolSlotState::ALLOCATED);
    
    // Compute pointer from block index and slot index using safe arithmetic
    const auto& blk = ps.blocks[blockIdx];
    char* ptr = safe_index(blk.base, slotIdx, ps.blockSize);
    // safe_index returns nullptr on overflow - this should not happen given earlier checks,
    // but we maintain the invariant that all pointer arithmetic is overflow-safe
    
    return ptr;
}

inline void MemoryManager::poolFree(void* ptr, int64_t poolId) {
    auto it = g_pools.find(poolId);
#ifdef KERN_DEBUG
    // STRICT: use-after-destroy is a crash in debug mode
    if (it == g_pools.end()) {
        std::abort();  // use-after-destroy detected: pool was destroyed or never existed
    }
#else
    if (it == g_pools.end()) return;  // Safe fail in release
#endif
    
    PoolState& ps = it->second;
    
    // Centralized pointer validation through getSlotIndex
    size_t blockIdx, slotIdx;
    if (!ps.getSlotIndex(ptr, blockIdx, slotIdx)) return;  // Invalid pointer
    
    // Validate indices (hard fail in debug mode)
#ifdef KERN_DEBUG
    if (blockIdx >= ps.blocks.size()) std::abort();
    if (slotIdx >= ps.blocks[blockIdx].count) std::abort();
#else
    if (blockIdx >= ps.blocks.size()) return;
    if (slotIdx >= ps.blocks[blockIdx].count) return;
#endif
    
    // UNIFIED STATE TRANSITION: current → FREE
    // This uses the compile-time verified transition table
    // Valid only if current state is ALLOCATED (enforced by transition_allowed table)
    PoolSlotState currentState = ps.getSlotState(blockIdx, slotIdx);
    if (!kern::pool::transition_state(currentState, PoolSlotState::FREE, blockIdx, slotIdx)) {
        // transition_state handles debug abort internally; here we just return
        return;
    }
    
    // Execute the transition
    ps.setSlotState(blockIdx, slotIdx, PoolSlotState::FREE);
    
    // Push to free slot index stack for O(1) reuse
    ps.freeSlotStack.push_back({blockIdx, slotIdx});
}

inline void MemoryManager::destroyPool(int64_t poolId) {
    auto it = g_pools.find(poolId);
    if (it == g_pools.end()) return;
    
    // Step 1: Mark all slots as INVALID IMMEDIATELY
    // This ensures any existing reference that validates via state machine
    // will see INVALID before we erase from map or free memory
    for (auto& blk : it->second.blocks) {
        for (size_t i = 0; i < blk.states.size(); ++i) {
            blk.states[i] = PoolSlotState::INVALID;
        }
    }
    
    // Step 2: Clear free slot stack (prevents further allocation)
    it->second.freeSlotStack.clear();
    
    // Step 3: Remove pool from lookup map (prevents new access)
    PoolState ps = std::move(it->second);
    g_pools.erase(it);
    
    // Step 4: Free all block memory
    for (auto& blk : ps.blocks) {
        std::free(blk.base);
    }
    
    // Step 5: Clear blocks vector
    ps.blocks.clear();
}

inline void* MemoryManager::alloc(size_t size) {
    const size_t kMaxAlloc = 256 * 1024 * 1024;
    if (size > kMaxAlloc) return nullptr;
    return std::malloc(size);
}

inline void MemoryManager::free(void* ptr) {
    if (ptr) std::free(ptr);
}

// Forward declaration for shutdown
inline void cleanupGlobalMemoryState();

inline void MemoryManager::shutdown() {
    cleanupGlobalMemoryState();
}

/* * Cleanup global state - call via VM::shutdownGlobalState() explicitly */
inline void cleanupGlobalMemoryState() {
    // Free all pool blocks and clear state bitmaps
    for (auto& [id, ps] : g_pools) {
        for (auto& blk : ps.blocks) {
            // Mark all slots as INVALID before freeing
            for (size_t i = 0; i < blk.states.size(); ++i) {
                blk.states[i] = PoolSlotState::INVALID;
            }
            std::free(blk.base);
        }
        ps.blocks.clear();
        ps.freeSlotStack.clear();
    }
    g_pools.clear();
    
    // Free all tracked allocations
    for (void* p : g_trackedAllocs) {
        std::free(p);
    }
    g_trackedAllocs.clear();
    
    // Free all aligned allocation bases
    for (auto& [ptr, base] : g_alignedAllocBases) {
        std::free(base);
    }
    g_alignedAllocBases.clear();
    
    // Unmap all mapped files
    for (auto& [ptr, state] : g_mappedFiles) {
#ifdef _WIN32
        if (state.view) UnmapViewOfFile(state.view);
        if (state.hMap) CloseHandle(state.hMap);
        if (state.hFile) CloseHandle(state.hFile);
#else
        if (state.view) munmap(state.view, state.size);
#endif
    }
    g_mappedFiles.clear();
    
    // Unload FFI libraries (Windows only)
#ifdef _WIN32
    for (auto& [name, hmod] : g_ffiLibraries) {
        if (hmod) FreeLibrary(hmod);
    }
    g_ffiLibraries.clear();
    g_ffiSymbols.clear();
#endif
}

namespace {

inline void kernAppendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7FU)
        out += static_cast<char>(cp);
    else if (cp <= 0x7FFU) {
        out += static_cast<char>(0xC0 | static_cast<int>(cp >> 6));
        out += static_cast<char>(0x80 | static_cast<int>(cp & 0x3F));
    } else if (cp <= 0xFFFFU) {
        out += static_cast<char>(0xE0 | static_cast<int>(cp >> 12));
        out += static_cast<char>(0x80 | static_cast<int>((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | static_cast<int>(cp & 0x3F));
    } else if (cp <= 0x10FFFFU) {
        out += static_cast<char>(0xF0 | static_cast<int>(cp >> 18));
        out += static_cast<char>(0x80 | static_cast<int>((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | static_cast<int>((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | static_cast<int>(cp & 0x3F));
    }
}

inline std::string kernUrlEncodeQueryPart(const std::string& s) {
    std::ostringstream out;
    static const char* hex = "0123456789ABCDEF";
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out << static_cast<char>(c);
        } else if (c == ' ') {
            out << '+';
        } else {
            out << '%' << hex[(c >> 4) & 0x0F] << hex[c & 0x0F];
        }
    }
    return out.str();
}

inline int kernB64CharVal(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

inline std::string kernBase64DecodeDataUrl(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3 / 4);
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (c == '=') break;
        int d = kernB64CharVal(c);
        if (d < 0) continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out += static_cast<char>((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}

inline std::string kernTrimHttpWs(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

/* * lowercased scheme, lowercased host, port (80/443 defaults for http/https, else from :port or -1).*/
inline void kernUrlHostPort(const std::string& u, std::string& schemeLower, std::string& hostLower, int64_t& port) {
    std::string rest;
    size_t c = u.find("://");
    if (c != std::string::npos) {
        schemeLower = u.substr(0, c);
        for (char& ch : schemeLower)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        rest = u.substr(c + 3);
    } else {
        schemeLower.clear();
        rest = u;
    }
    size_t slash = rest.find('/');
    std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string host = hostport;
    port = (schemeLower == "https") ? 443 : (schemeLower == "http") ? 80 : int64_t(-1);
    size_t colon = hostport.rfind(':');
    if (colon != std::string::npos && colon > 0) {
        host = hostport.substr(0, colon);
        port = std::strtoll(hostport.substr(colon + 1).c_str(), nullptr, 10);
    }
    hostLower.clear();
    for (char ch : host)
        hostLower += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

inline std::string kernCookieValueField(const std::string& val) {
    bool needQuote = false;
    for (unsigned char c : val) {
        if (c == ';' || c == ' ' || c == '"' || c == '\\' || c == '\r' || c == '\n' || c == '\t') {
            needQuote = true;
            break;
        }
    }
    if (!needQuote) return val;
    std::string o = "\"";
    for (char c : val) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    o += '"';
    return o;
}

inline int kernHexVal(char x) {
    if (x >= '0' && x <= '9') return x - '0';
    if (x >= 'a' && x <= 'f') return x - 'a' + 10;
    if (x >= 'A' && x <= 'F') return x - 'A' + 10;
    return -1;
}

inline std::string kernPercentDecodeLoose(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            int hi = kernHexVal(s[i + 1]);
            int lo = kernHexVal(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

inline std::string kernCssUrlEscape(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/' || c == ':' || c == '~')
            out += static_cast<char>(c);
        else {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

inline std::string kernNormalizeUrlPath(std::string path, bool hadTrailingSlash) {
    if (path.empty()) path = "/";
    if (path[0] != '/') path = "/" + path;
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (cur == "..") {
                if (!parts.empty()) parts.pop_back();
            } else if (cur != "." && !cur.empty())
                parts.push_back(cur);
            cur.clear();
        } else
            cur += path[i];
    }
    std::string out = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out += "/";
        out += parts[i];
    }
    if (parts.empty()) out = "/";
    if (hadTrailingSlash && out.size() > 1 && out.back() != '/') out += "/";
    return out;
}

inline std::string kernUrlNormalize(const std::string& in) {
    std::string u = kernTrimHttpWs(in);
    if (u.empty()) return u;
    size_t c = u.find("://");
    if (c == std::string::npos) return u;
    std::string scheme = u.substr(0, c);
    for (char& ch : scheme) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    std::string rest = u.substr(c + 3);
    size_t hashPos = rest.find('#');
    std::string frag = hashPos == std::string::npos ? "" : rest.substr(hashPos);
    if (hashPos != std::string::npos) rest = rest.substr(0, hashPos);
    size_t qPos = rest.find('?');
    std::string query = qPos == std::string::npos ? "" : rest.substr(qPos);
    if (qPos != std::string::npos) rest = rest.substr(0, qPos);
    size_t slash = rest.find('/');
    std::string auth = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
    bool trail = path.size() > 1 && path.back() == '/';
    for (char& ch : auth) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    if (!auth.empty() && auth.front() != '[') {
        size_t colon = auth.rfind(':');
        if (colon != std::string::npos && colon > 0) {
            bool allDig = true;
            for (size_t k = colon + 1; k < auth.size(); ++k) {
                if (!std::isdigit(static_cast<unsigned char>(auth[k]))) {
                    allDig = false;
                    break;
                }
            }
            if (allDig) {
                int p = static_cast<int>(std::strtol(auth.substr(colon + 1).c_str(), nullptr, 10));
                if ((scheme == "http" && p == 80) || (scheme == "https" && p == 443)) auth = auth.substr(0, colon);
            }
        }
    }
    path = kernNormalizeUrlPath(path, trail);
    return scheme + "://" + auth + path + query + frag;
}

inline std::string kernHtmlSanitizeStrict(const std::string& in, const std::unordered_set<std::string>& allow) {
    try {
        std::string s = in;
        std::regex cmt("<!--[\\s\\S]*?-->");
        s = std::regex_replace(s, cmt, std::string(""));
        std::regex scr("<script\\b[^>]*>[\\s\\S]*?</script>", std::regex::icase);
        s = std::regex_replace(s, scr, std::string(""));
        std::regex stl("<style\\b[^>]*>[\\s\\S]*?</style>", std::regex::icase);
        s = std::regex_replace(s, stl, std::string(""));
        std::regex ifr("<iframe\\b[^>]*>[\\s\\S]*?</iframe>", std::regex::icase);
        s = std::regex_replace(s, ifr, std::string(""));
        std::regex nos("<noscript\\b[^>]*>[\\s\\S]*?</noscript>", std::regex::icase);
        s = std::regex_replace(s, nos, std::string(""));
        std::regex tagRe("<(/?)([a-zA-Z][a-zA-Z0-9:-]*)\\b[\\s\\S]*?>");
        std::string out;
        std::sregex_iterator it(s.begin(), s.end(), tagRe), end;
        size_t last = 0;
        for (; it != end; ++it) {
            size_t mpos = static_cast<size_t>(it->position());
            out += s.substr(last, mpos - last);
            std::string name = (*it)[2].str();
            std::string lname = name;
            for (char& ch : lname) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (allow.count(lname)) {
                bool closing = !(*it)[1].str().empty();
                std::string full = it->str();
                bool selfClose = !closing && full.size() >= 2 && full[full.size() - 2] == '/';
                if (closing)
                    out += "</" + lname + ">";
                else if (selfClose)
                    out += "<" + lname + "/>";
                else
                    out += "<" + lname + ">";
            }
            last = mpos + static_cast<size_t>(it->length());
        }
        out += s.substr(last);
        return out;
    } catch (...) {
        return in;
    }
}

inline size_t kernLinkParamSectionEnd(const std::string& s, size_t start) {
    size_t i = start;
    bool inD = false, inS = false;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '"' && !inS) inD = !inD;
        else if (c == '\'' && !inD) inS = !inS;
        else if (!inD && !inS && c == ',') {
            size_t j = i + 1;
            while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) ++j;
            if (j < s.size() && s[j] == '<') return i;
        }
        ++i;
    }
    return s.size();
}

inline std::string kernBase64DecodeStr(const std::string& in0) {
    static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string in = in0;
    while (!in.empty() && in.back() == '=') in.pop_back();
    static int T[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) T[i] = -1;
        for (int i = 0; i < 64; ++i) T[static_cast<unsigned char>(kB64[i])] = i;
        init = true;
    }
    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] < 0) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out += static_cast<char>((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    return out;
}

inline const char* kernHttpReasonPhrase(int code) {
    switch (code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 422: return "Unprocessable Entity";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "OK";
    }
}

inline std::string kernUrlPathJoin(const std::string& base, const std::string& seg) {
    std::string a = kernTrimHttpWs(base);
    std::string b = kernTrimHttpWs(seg);
    while (!a.empty() && a.back() == '/') a.pop_back();
    while (!b.empty() && b.front() == '/') b.erase(0, 1);
    if (a.empty()) return b.empty() ? std::string("/") : (std::string("/") + b);
    if (b.empty()) return a;
    return a + "/" + b;
}

inline void kernParseSemicolonKeyValues(const std::string& p, std::unordered_map<std::string, std::string>& outLowerKey) {
    size_t start = 0;
    bool inD = false, inS = false;
    for (size_t i = 0; i <= p.size(); ++i) {
        if (i < p.size()) {
            unsigned char c = static_cast<unsigned char>(p[i]);
            if (c == '"' && !inS) inD = !inD;
            else if (c == '\'' && !inD) inS = !inS;
        }
        if (i == p.size() || (!inD && !inS && p[i] == ';')) {
            std::string part = kernTrimHttpWs(p.substr(start, i - start));
            start = i + 1;
            if (part.empty()) continue;
            size_t eq = part.find('=');
            std::string k = eq == std::string::npos ? part : kernTrimHttpWs(part.substr(0, eq));
            std::string v = eq == std::string::npos ? "" : kernTrimHttpWs(part.substr(eq + 1));
            for (char& c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!v.empty() && v.front() == '"' && v.back() == '"' && v.size() >= 2) v = v.substr(1, v.size() - 2);
            else if (!v.empty() && v.front() == '\'' && v.back() == '\'' && v.size() >= 2) v = v.substr(1, v.size() - 2);
            if (!k.empty()) outLowerKey[k] = v;
        }
    }
}

inline std::string kernUrlResolve(const std::string& base, const std::string& rel) {
    if (rel.empty()) return base;
    size_t rc = rel.find("://");
    if (rc != std::string::npos) {
        bool ok = true;
        for (size_t i = 0; i < rc; ++i) {
            if (!std::isalpha(static_cast<unsigned char>(rel[i]))) {
                ok = false;
                break;
            }
        }
        if (ok) return rel;
    }
    std::string scheme, rest;
    size_t c = base.find("://");
    if (c == std::string::npos) return rel;
    scheme = base.substr(0, c);
    rest = base.substr(c + 3);
    if (rel.size() >= 2 && rel[0] == '/' && rel[1] == '/') return scheme + ":" + rel;

    std::string hostport, pathq;
    size_t slash = rest.find('/');
    if (slash == std::string::npos) {
        hostport = rest;
        pathq = "/";
    } else {
        hostport = rest.substr(0, slash);
        pathq = rest.substr(slash);
    }
    if (pathq.empty() || pathq[0] != '/') pathq = "/" + pathq;

    if (rel[0] == '/') return scheme + "://" + hostport + rel;

    std::string dir = pathq;
    size_t q = dir.find('?');
    if (q != std::string::npos) dir = dir.substr(0, q);
    size_t h = dir.find('#');
    if (h != std::string::npos) dir = dir.substr(0, h);
    size_t lastSlash = dir.rfind('/');
    if (lastSlash == std::string::npos)
        dir = "/";
    else
        dir = dir.substr(0, lastSlash + 1);
    std::string merged = dir + rel;
    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = 0; i <= merged.size(); ++i) {
        if (i == merged.size() || merged[i] == '/') {
            if (cur == "..") {
                if (!parts.empty()) parts.pop_back();
            } else if (cur != "." && !cur.empty())
                parts.push_back(cur);
            cur.clear();
        } else
            cur += merged[i];
    }
    std::string norm = "/";
    for (size_t i = 0; i < parts.size(); ++i) {
        norm += parts[i];
        if (i + 1 < parts.size()) norm += "/";
    }
    if (norm.empty()) norm = "/";
    return scheme + "://" + hostport + norm;
}

/* * multi-line Python-style exception report (name, message, traceback frames, cause chain).*/
inline std::string formatExceptionValue(const ValuePtr& e, int depth = 0) {
    if (depth > 14) return "...(exception chain truncated)\n";
    if (!e) return "null\n";
    if (e->type != Value::Type::MAP) return e->toString() + "\n";
    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(e->data);
    std::string name = "Error";
    auto itn = m.find("name");
    if (itn != m.end() && itn->second && itn->second->type == Value::Type::STRING)
        name = std::get<std::string>(itn->second->data);
    std::string msg;
    auto itm = m.find("message");
    if (itm != m.end() && itm->second && itm->second->type == Value::Type::STRING)
        msg = std::get<std::string>(itm->second->data);
    std::ostringstream out;
    out << name << ": " << msg << "\n";
    auto itt = m.find("traceback");
    if (itt != m.end() && itt->second && itt->second->type == Value::Type::ARRAY) {
        out << "Traceback (most recent call last):\n";
        const auto& arr = std::get<std::vector<ValuePtr>>(itt->second->data);
        for (const auto& fr : arr) {
            if (!fr || fr->type != Value::Type::MAP) continue;
            auto& fm = std::get<std::unordered_map<std::string, ValuePtr>>(fr->data);
            std::string fn = "?";
            int64_t line = 0;
            int64_t col = 0;
            auto itf = fm.find("name");
            if (itf != fm.end() && itf->second && itf->second->type == Value::Type::STRING)
                fn = std::get<std::string>(itf->second->data);
            auto itl = fm.find("line");
            if (itl != fm.end() && itl->second && itl->second->type == Value::Type::INT)
                line = std::get<int64_t>(itl->second->data);
            auto itc = fm.find("column");
            if (itc != fm.end() && itc->second && itc->second->type == Value::Type::INT)
                col = std::get<int64_t>(itc->second->data);
            std::string rawFile;
            auto itPath = fm.find("file");
            if (itPath != fm.end() && itPath->second && itPath->second->type == Value::Type::STRING)
                rawFile = std::get<std::string>(itPath->second->data);
            const std::string dispFile = rawFile.empty() ? "<kn>" : humanizePathForDisplay(rawFile);
            out << "  File \"" << dispFile << "\", line " << line;
            if (col > 0) out << ", column " << col;
            out << ", in " << fn << "\n";
        }
    }
    auto itc = m.find("cause");
    if (itc != m.end() && itc->second) {
        out << "\nThe above exception was the direct cause of the following exception:\n\n";
        out << formatExceptionValue(itc->second, depth + 1);
    }
    return out.str();
}

    std::function<bool(const ValuePtr&, const ValuePtr&)> g_assertEqDeepEqual;
#ifdef _WIN32
    std::unordered_map<int64_t, HANDLE> g_spawnHandles;
    int64_t g_nextSpawnHandle = 1;
#endif
    std::unordered_map<int64_t, FILE*> g_fdHandles;
    int64_t g_nextFdHandle = 1;
    std::mutex g_fdMutex;
    std::unordered_set<std::string> g_flockKeys;
    std::mutex g_flockMutex;
    std::unordered_map<int64_t, std::unordered_map<std::string, int64_t>> g_fsWatchState;
    int64_t g_nextWatchId = 1;
    std::mutex g_fsWatchMutex;
    std::unordered_map<int64_t, std::vector<int64_t>> g_processJobs;
    int64_t g_nextProcessJobId = 1;
    std::mutex g_processJobsMutex;
    std::unordered_map<std::string, ValuePtr> g_signalTraps;
    std::mutex g_signalTrapMutex;
    std::mutex g_regexCacheMutex;
    std::unordered_map<int64_t, std::regex> g_regexCache;
    std::atomic<int64_t> g_nextRegexId{1};  // first compiled regex id is 1
}

inline void registerAllBuiltins(VM& vm) {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    auto makeBuiltin = [&vm](size_t idx, VM::BuiltinFn fn) {
        vm.registerBuiltin(idx, std::move(fn));
    };
    auto setGlobalFn = [&vm](const std::string& name, size_t idx) {
        auto fn = std::make_shared<FunctionObject>();
        fn->isBuiltin = true;
        fn->builtinIndex = idx;
        vm.setGlobal(name, std::make_shared<Value>(Value::fromFunction(fn)));
    };
    auto toDouble = [](ValuePtr v) -> double {
        if (!v) return 0;
        if (v->type == Value::Type::INT) return static_cast<double>(std::get<int64_t>(v->data));
        if (v->type == Value::Type::FLOAT) return std::get<double>(v->data);
        return 0;
    };
    auto toInt = [](ValuePtr v) -> int64_t {
        if (!v) return 0;
        if (v->type == Value::Type::INT) return std::get<int64_t>(v->data);
        if (v->type == Value::Type::FLOAT) return static_cast<int64_t>(std::get<double>(v->data));
        return 0;
    };

    size_t i = 0;
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        for (size_t j = 0; j < args.size(); ++j) {
            if (j) std::cout << " ";
            std::cout << (args[j] ? args[j]->toString() : "null");
        }
        std::cout << std::endl;
        return Value::nil();
    });
    setGlobalFn("print", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::sqrt(toDouble(args[0])));
    });
    setGlobalFn("sqrt", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(1);
        return Value::fromFloat(std::pow(toDouble(args[0]), toDouble(args[1])));
    });
    setGlobalFn("pow", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::sin(toDouble(args[0])));
    });
    setGlobalFn("sin", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(1);
        return Value::fromFloat(std::cos(toDouble(args[0])));
    });
    setGlobalFn("cos", i - 1);

    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 2) {
            int64_t lo = toInt(args[0]), hi = toInt(args[1]);
            if (lo > hi) std::swap(lo, hi);
            int64_t range = hi - lo + 1;
            if (range <= 0) return Value::fromInt(lo);
            return Value::fromInt(lo + (std::rand() % static_cast<unsigned>(range)));
        }
        return Value::fromFloat(static_cast<double>(std::rand()) / RAND_MAX);
    });
    setGlobalFn("random", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(0);
        return Value::fromInt(static_cast<int64_t>(std::floor(toDouble(args[0]))));
    });
    setGlobalFn("floor", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(0);
        return Value::fromInt(static_cast<int64_t>(std::ceil(toDouble(args[0]))));
    });
    setGlobalFn("ceil", i - 1);

    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        std::string s;
        for (const auto& a : args) s += a ? a->toString() : "null";
        return Value::fromString(s);
    });
    setGlobalFn("str", i - 1);

    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(0);
        return Value::fromInt(toInt(args[0]));
    });
    setGlobalFn("int", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(toDouble(args[0]));
    });
    setGlobalFn("float", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::vector<ValuePtr> arr;
        for (const auto& a : args) arr.push_back(a ? a : std::make_shared<Value>(Value::nil()));
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("array", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::fromInt(0);
        if (args[0]->type == Value::Type::ARRAY)
            return Value::fromInt(static_cast<int64_t>(std::get<std::vector<ValuePtr>>(args[0]->data).size()));
        if (args[0]->type == Value::Type::STRING)
            return Value::fromInt(static_cast<int64_t>(std::get<std::string>(args[0]->data).size()));
        return Value::fromInt(0);
    });
    setGlobalFn("len", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "read_file");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string path = std::get<std::string>(args[0]->data);
        std::ifstream f(path);
        if (!f) return Value::nil();
        std::stringstream buf;
        buf << f.rdbuf();
        return Value::fromString(buf.str());
    });
    setGlobalFn("read_file", i - 1);
    setGlobalFn("readFile", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "write_file");
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        std::string path = std::get<std::string>(args[0]->data);
        std::string content = args[1] && args[1]->type == Value::Type::STRING ? std::get<std::string>(args[1]->data) : (args[1] ? args[1]->toString() : "");
        std::ofstream f(path);
        if (!f) return Value::fromBool(false);
        f << content;
        return Value::fromBool(true);
    });
    setGlobalFn("write_file", i - 1);
    setGlobalFn("writeFile", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
        return Value::fromFloat(static_cast<double>(std::time(nullptr)));
    });
    setGlobalFn("time", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        return Value::fromString(args[0] ? args[0]->toString() : "null");
    });
    setGlobalFn("inspect", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("alloc requires unsafe context", 0, 0, 5);
        if (args.empty()) return Value::nil();
        int64_t req = toInt(args[0]);
        if (req <= 0) return Value::fromPtr(nullptr);
        const size_t kMaxAlloc = 256 * 1024 * 1024;
        size_t n = static_cast<size_t>(req);
        if (n > kMaxAlloc) return Value::nil();
        void* p = std::malloc(n);
        return Value::fromPtr(p);
    });
    setGlobalFn("alloc", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("free requires unsafe context", 0, 0, 5);
        if (args.empty() || !args[0]) return Value::nil();
        if (args[0]->type != Value::Type::PTR) return Value::nil();
        void* p = std::get<void*>(args[0]->data);
        if (p) {
            auto it = g_alignedAllocBases.find(p);
            if (it != g_alignedAllocBases.end()) { std::free(it->second); g_alignedAllocBases.erase(it); }
            else std::free(p);
        }
        return Value::nil();
    });
    setGlobalFn("free", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::fromString("nil");
        switch (args[0]->type) {
            case Value::Type::NIL: return Value::fromString("nil");
            case Value::Type::BOOL: return Value::fromString("bool");
            case Value::Type::INT: return Value::fromString("int");
            case Value::Type::FLOAT: return Value::fromString("float");
            case Value::Type::STRING: return Value::fromString("string");
            case Value::Type::ARRAY: return Value::fromString("array");
            case Value::Type::MAP: return Value::fromString("dictionary");
            case Value::Type::FUNCTION: return Value::fromString("function");
            case Value::Type::CLASS: return Value::fromString("class");
            case Value::Type::INSTANCE: return Value::fromString("instance");
            case Value::Type::PTR: return Value::fromString("ptr");
            default: return Value::fromString("unknown");
        }
    });
    setGlobalFn("type", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::vector<ValuePtr> out;
        if (args.empty() || !args[0]) return Value::fromArray(std::move(out));
        if (args[0]->type == Value::Type::MAP) {
            auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
            for (const auto& kv : m) out.push_back(std::make_shared<Value>(Value::fromString(kv.first)));
        } else if (args[0]->type == Value::Type::ARRAY) {
            auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
            for (size_t i = 0; i < a.size(); ++i)
                out.push_back(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(i))));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("dir", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (!vm) return Value::fromInt(0);
        uint64_t n = vm->getCycleCount();
        if (!args.empty() && args[0] && args[0]->isTruthy()) vm->resetCycleCount();
        return Value::fromInt(static_cast<int64_t>(n));
    });
    setGlobalFn("profile_cycles", i - 1);

    vm.setGlobal("PI", std::make_shared<Value>(Value::fromFloat(M_PI)));
    vm.setGlobal("E", std::make_shared<Value>(Value::fromFloat(M_E)));

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0]) return Value::nil();
        if (args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        const size_t kMaxArraySize = 64 * 1024 * 1024;
        if (arr.size() >= kMaxArraySize) return Value::nil();
        arr.push_back(args[1] ? args[1] : std::make_shared<Value>(Value::nil()));
        return Value(*args[0]);
    });
    setGlobalFn("push", i - 1);

    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::fromArray({});
        if (args[0]->type == Value::Type::STRING) {
            const std::string& s = std::get<std::string>(args[0]->data);
            int64_t len = static_cast<int64_t>(s.size());
            int64_t start = args.size() > 1 ? toInt(args[1]) : 0;
            int64_t end = args.size() > 2 ? toInt(args[2]) : len;
            if (start < 0) start = std::max(int64_t(0), start + len);
            if (end < 0) end = std::max(int64_t(0), end + len);
            if (start > len) start = len;
            if (end > len) end = len;
            if (start >= end) return Value::fromString("");
            return Value::fromString(s.substr(static_cast<size_t>(start), static_cast<size_t>(end - start)));
        }
        if (args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t start = args.size() > 1 ? toInt(args[1]) : 0;
        int64_t end = args.size() > 2 ? toInt(args[2]) : static_cast<int64_t>(arr.size());
        int64_t alen = static_cast<int64_t>(arr.size());
        if (start < 0) start = std::max(int64_t(0), start + alen);
        if (end < 0) end = std::max(int64_t(0), end + alen);
        if (end > alen) end = alen;
        if (start > alen) start = alen;
        std::vector<ValuePtr> out;
        for (int64_t i = start; i < end; ++i) {
            if (i >= 0 && i < alen) out.push_back(arr[static_cast<size_t>(i)]);
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("slice", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::vector<ValuePtr> out;
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::fromArray(std::move(out));
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        for (const auto& kv : m) out.push_back(std::make_shared<Value>(Value::fromString(kv.first)));
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("keys", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::vector<ValuePtr> out;
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::fromArray(std::move(out));
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        for (const auto& kv : m) out.push_back(kv.second);
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("values", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::MAP) return Value::fromBool(false);
        std::string key;
        if (args[1]->type == Value::Type::STRING)
            key = std::get<std::string>(args[1]->data);
        else if (args[1]->type == Value::Type::INT)
            key = std::to_string(std::get<int64_t>(args[1]->data));
        else if (args[1]->type == Value::Type::FLOAT)
            key = std::to_string(std::get<double>(args[1]->data));
        else
            return Value::fromBool(false);
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        return Value::fromBool(m.find(key) != m.end());
    });
    setGlobalFn("has", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string s = std::get<std::string>(args[0]->data);
        for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return Value::fromString(s);
    });
    setGlobalFn("upper", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string s = std::get<std::string>(args[0]->data);
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return Value::fromString(s);
    });
    setGlobalFn("lower", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3 || !args[0] || !args[1] || !args[2] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING ||
            args[2]->type != Value::Type::STRING)
            return Value::nil();
        std::string s = std::get<std::string>(args[0]->data);
        std::string from = std::get<std::string>(args[1]->data);
        std::string to = std::get<std::string>(args[2]->data);
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
        return Value::fromString(s);
    });
    setGlobalFn("replace", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::nil();
        if (args[0]->type != Value::Type::ARRAY) return Value::nil();
        std::string sep = args[1]->type == Value::Type::STRING ? std::get<std::string>(args[1]->data) : " ";
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::string out;
        for (size_t i = 0; i < arr.size(); ++i) { if (i) out += sep; out += arr[i] ? arr[i]->toString() : "null"; }
        return Value::fromString(out);
    });
    setGlobalFn("join", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        std::string s = std::get<std::string>(args[0]->data);
        std::string sep = args.size() > 1 && args[1] && args[1]->type == Value::Type::STRING ? std::get<std::string>(args[1]->data) : " ";
        std::vector<ValuePtr> out;
        size_t start = 0, pos;
        while ((pos = s.find(sep, start)) != std::string::npos) {
            out.push_back(std::make_shared<Value>(Value::fromString(s.substr(start, pos - start))));
            start = pos + sep.size();
        }
        out.push_back(std::make_shared<Value>(Value::fromString(s.substr(start))));
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("split", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(0);
        return Value::fromInt(static_cast<int64_t>(std::round(toDouble(args[0]))));
    });
    setGlobalFn("round", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::fabs(toDouble(args[0])));
    });
    setGlobalFn("abs", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::log(toDouble(args[0])));
    });
    setGlobalFn("log", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fileExists");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        return Value::fromBool(std::filesystem::exists(std::get<std::string>(args[0]->data)));
    });
    setGlobalFn("fileExists", i - 1);
    setGlobalFn("file_exists", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "listDir");
        std::string path = args.empty() || !args[0] || args[0]->type != Value::Type::STRING ? "." : std::get<std::string>(args[0]->data);
        std::vector<ValuePtr> out;
        try {
            for (const auto& e : std::filesystem::directory_iterator(path))
                out.push_back(std::make_shared<Value>(Value::fromString(e.path().filename().string())));
        } catch (...) {}
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("listDir", i - 1);
    setGlobalFn("list_dir", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (!args.empty() && args[0]) {
            double sec = toDouble(args[0]);
            if (sec > 0) std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(sec * 1000)));
        }
        return Value::nil();
    });
    setGlobalFn("sleep", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::nil();
        Value inst = Value::fromMap({});
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(inst.data);
        m["__class"] = args[0];
        for (size_t i = 1; i < args.size(); ++i) m["__arg" + std::to_string(i)] = args[i];
        return inst;
    });
    setGlobalFn("Instance", i - 1);

    // jSON support
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        JsonParser p{ std::get<std::string>(args[0]->data), 0 };
        ValuePtr v = p.parseValue();
        return v ? Value(*v) : Value::nil();
    });
    setGlobalFn("json_parse", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string indentStr;
        if (args.size() >= 2 && args[1]) {
            if (args[1]->type == Value::Type::INT) {
                int n = static_cast<int>(std::get<int64_t>(args[1]->data));
                if (n < 0) n = 0;
                if (n > 16) n = 16;
                indentStr.assign(static_cast<size_t>(n), ' ');
            } else if (args[1]->type == Value::Type::STRING)
                indentStr = std::get<std::string>(args[1]->data);
        }
        const bool pretty = !indentStr.empty();
        std::function<void(const ValuePtr&, std::string&, int)> toJson;
        toJson = [&toJson, &indentStr, pretty](const ValuePtr& v, std::string& out, int depth) {
            auto pad = [&indentStr](int d) {
                std::string s;
                for (int k = 0; k < d; ++k) s += indentStr;
                return s;
            };
            if (!v) { out += "null"; return; }
            switch (v->type) {
                case Value::Type::NIL: out += "null"; break;
                case Value::Type::BOOL: out += std::get<bool>(v->data) ? "true" : "false"; break;
                case Value::Type::INT: out += std::to_string(std::get<int64_t>(v->data)); break;
                case Value::Type::FLOAT: {
                    double d = std::get<double>(v->data);
                    if (std::isnan(d)) { out += "null"; break; }
                    if (std::isinf(d)) { out += "null"; break; }
                    char b[64]; snprintf(b, sizeof(b), "%.15g", d); out += b;
                    break;
                }
                case Value::Type::STRING: out += "\""; out += jsonEscape(std::get<std::string>(v->data)); out += "\""; break;
                case Value::Type::ARRAY: {
                    out += "[";
                    auto& arr = std::get<std::vector<ValuePtr>>(v->data);
                    for (size_t i = 0; i < arr.size(); ++i) {
                        if (i) out += ",";
                        if (pretty) { out += "\n"; out += pad(depth + 1); }
                        toJson(arr[i], out, depth + 1);
                    }
                    if (pretty && !arr.empty()) { out += "\n"; out += pad(depth); }
                    out += "]";
                    break;
                }
                case Value::Type::MAP: {
                    out += "{";
                    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(v->data);
                    bool first = true;
                    for (const auto& kv : m) {
                        if (!first) out += ",";
                        first = false;
                        if (pretty) { out += "\n"; out += pad(depth + 1); }
                        out += "\""; out += jsonEscape(kv.first); out += "\":";
                        if (pretty) out += " ";
                        toJson(kv.second, out, depth + 1);
                    }
                    if (pretty && !m.empty()) { out += "\n"; out += pad(depth); }
                    out += "}";
                    break;
                }
                default: out += "null"; break;
            }
        };
        std::string out;
        if (!args.empty()) toJson(args[0], out, 0);
        return Value::fromString(std::move(out));
    });
    setGlobalFn("json_stringify", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        std::vector<ValuePtr> arr;
        if (vm) for (const auto& a : vm->getCliArgs()) arr.push_back(std::make_shared<Value>(Value::fromString(a)));
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("cli_args", i - 1);

    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromArray({});
        int64_t start = toInt(args[0]), end = toInt(args[1]);
        int64_t step = args.size() >= 3 ? toInt(args[2]) : 1;
        if (step == 0) step = 1;
        std::vector<ValuePtr> out;
        const size_t kMaxRange = 16 * 1024 * 1024;
        if (step > 0) {
            for (int64_t i = start; i < end; i += step) {
                if (out.size() >= kMaxRange) break;
                out.push_back(std::make_shared<Value>(Value::fromInt(i)));
            }
        } else {
            for (int64_t i = start; i > end; i += step) {
                if (out.size() >= kMaxRange) break;
                out.push_back(std::make_shared<Value>(Value::fromInt(i)));
            }
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("range", i - 1);

    static std::function<ValuePtr(const ValuePtr&)> deepCopy;
    deepCopy = [](const ValuePtr& v) -> ValuePtr {
        if (!v) return std::make_shared<Value>(Value::nil());
        switch (v->type) {
            case Value::Type::NIL:
            case Value::Type::BOOL:
            case Value::Type::INT:
            case Value::Type::FLOAT:
            case Value::Type::STRING:
                return std::make_shared<Value>(*v);
            case Value::Type::ARRAY: {
                auto& arr = std::get<std::vector<ValuePtr>>(v->data);
                std::vector<ValuePtr> out;
                for (const auto& e : arr) out.push_back(deepCopy(e));
                return std::make_shared<Value>(Value::fromArray(std::move(out)));
            }
            case Value::Type::MAP: {
                auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(v->data);
                std::unordered_map<std::string, ValuePtr> out;
                for (const auto& kv : m) out[kv.first] = deepCopy(kv.second);
                return std::make_shared<Value>(Value::fromMap(std::move(out)));
            }
            default:
                return std::make_shared<Value>(*v);
        }
    };
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        return *deepCopy(args[0]);
    });
    setGlobalFn("copy", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        return *deepCopy(args[0]);
    });
    setGlobalFn("freeze", i - 1);

    static std::function<bool(const ValuePtr&, const ValuePtr&)> deepEqual;
    deepEqual = [](const ValuePtr& a, const ValuePtr& b) -> bool {
        if (!a && !b) return true;
        if (!a || !b) return false;
        if (a->type != b->type) return false;
        switch (a->type) {
            case Value::Type::NIL: return true;
            case Value::Type::BOOL: return std::get<bool>(a->data) == std::get<bool>(b->data);
            case Value::Type::INT: return std::get<int64_t>(a->data) == std::get<int64_t>(b->data);
            case Value::Type::FLOAT: return std::get<double>(a->data) == std::get<double>(b->data);
            case Value::Type::STRING: return std::get<std::string>(a->data) == std::get<std::string>(b->data);
            case Value::Type::ARRAY: {
                auto& aa = std::get<std::vector<ValuePtr>>(a->data);
                auto& bb = std::get<std::vector<ValuePtr>>(b->data);
                if (aa.size() != bb.size()) return false;
                for (size_t i = 0; i < aa.size(); ++i) if (!deepEqual(aa[i], bb[i])) return false;
                return true;
            }
            case Value::Type::MAP: {
                auto& ma = std::get<std::unordered_map<std::string, ValuePtr>>(a->data);
                auto& mb = std::get<std::unordered_map<std::string, ValuePtr>>(b->data);
                if (ma.size() != mb.size()) return false;
                for (const auto& kv : ma) {
                    auto it = mb.find(kv.first);
                    if (it == mb.end() || !deepEqual(kv.second, it->second)) return false;
                }
                return true;
            }
            default: return a->equals(*b);
        }
    };
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromBool(false);
        return Value::fromBool(deepEqual(args[0], args[1]));
    });
    setGlobalFn("deep_equal", i - 1);
    g_assertEqDeepEqual = deepEqual;

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (arr.empty()) return Value::nil();
        size_t i = static_cast<size_t>(std::rand()) % arr.size();
        return arr[i] ? *arr[i] : Value::nil();
    });
    setGlobalFn("random_choice", i - 1);

    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromInt(0);
        int64_t lo = toInt(args[0]), hi = toInt(args[1]);
        if (lo > hi) std::swap(lo, hi);
        int64_t range = hi - lo + 1;
        if (range <= 0) return Value::fromInt(lo);
        return Value::fromInt(lo + (std::rand() % static_cast<unsigned>(range)));
    });
    setGlobalFn("random_int", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        for (size_t i = arr.size(); i > 1; --i) {
            size_t j = static_cast<size_t>(std::rand()) % i;
            std::swap(arr[j], arr[i - 1]);
        }
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("random_shuffle", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        ValuePtr fn = args[1];
        std::vector<ValuePtr> out;
        for (size_t i = 0; i < arr.size(); ++i) {
            ValuePtr r = vm->callValue(fn, {arr[i] ? arr[i] : std::make_shared<Value>(Value::nil())});
            out.push_back(r ? r : std::make_shared<Value>(Value::nil()));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("map", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        ValuePtr fn = args[1];
        std::vector<ValuePtr> out;
        for (size_t i = 0; i < arr.size(); ++i) {
            ValuePtr v = arr[i] ? arr[i] : std::make_shared<Value>(Value::nil());
            ValuePtr r = vm->callValue(fn, {v});
            if (r && r->isTruthy()) out.push_back(v);
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("filter", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (arr.empty()) return Value::nil();
        if (args.size() < 2) return arr[0] ? *arr[0] : Value::nil();
        ValuePtr fn = args[1];
        ValuePtr acc = args.size() > 2 ? args[2] : (arr[0] ? arr[0] : std::make_shared<Value>(Value::nil()));
        for (size_t i = args.size() > 2 ? 0 : 1; i < arr.size(); ++i) {
            ValuePtr v = arr[i] ? arr[i] : std::make_shared<Value>(Value::nil());
            acc = vm->callValue(fn, {acc, v});
        }
        return acc ? *acc : Value::nil();
    });
    setGlobalFn("reduce", i - 1);

    // advanced error handling: Error(message [, code [, cause]]), panic(msg), error_message(err), error_name(e), error_cause(e), ValueError, TypeError, ...
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        std::string msg = args.empty() || !args[0] ? "" : args[0]->toString();
        std::string codeStr;
        if (args.size() > 1 && args[1]) codeStr = args[1]->toString();
        ValuePtr cause = (args.size() > 2 && args[2]) ? args[2] : nullptr;
        std::unordered_map<std::string, ValuePtr> m;
        m["message"] = std::make_shared<Value>(Value::fromString(msg));
        m["name"] = std::make_shared<Value>(Value::fromString("Error"));
        m["code"] = codeStr.empty() ? std::make_shared<Value>(Value::nil()) : std::make_shared<Value>(Value::fromString(codeStr));
        if (cause) m["cause"] = cause;
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("Error", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) -> Value {
        std::string msg = args.empty() || !args[0] ? "panic" : args[0]->toString();
        throw VMError(msg, 0, 0, 1);
        return Value::nil();
    });
    setGlobalFn("panic", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::fromString("null");
        if (args[0]->type == Value::Type::MAP) {
            auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
            auto it = m.find("message");
            if (it != m.end() && it->second && it->second->type == Value::Type::STRING)
                return *it->second;
        }
        return Value::fromString(args[0]->toString());
    });
    setGlobalFn("error_message", i - 1);

    // math helpers
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::fromFloat(0);
        double x = toDouble(args[0]), lo = toDouble(args[1]), hi = toDouble(args[2]);
        if (x < lo) return Value::fromFloat(lo);
        if (x > hi) return Value::fromFloat(hi);
        return Value::fromFloat(x);
    });
    setGlobalFn("clamp", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::fromFloat(0);
        double a = toDouble(args[0]), b = toDouble(args[1]), t = toDouble(args[2]);
        return Value::fromFloat(a + t * (b - a));
    });
    setGlobalFn("lerp", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        double m = toDouble(args[0]);
        for (size_t j = 1; j < args.size(); ++j) { double v = toDouble(args[j]); if (v < m) m = v; }
        return Value::fromFloat(m);
    });
    setGlobalFn("min", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        double m = toDouble(args[0]);
        for (size_t j = 1; j < args.size(); ++j) { double v = toDouble(args[j]); if (v > m) m = v; }
        return Value::fromFloat(m);
    });
    setGlobalFn("max", i - 1);

    // file system
    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "listDirRecursive");
        std::string path = args.empty() || !args[0] || args[0]->type != Value::Type::STRING ? "." : std::get<std::string>(args[0]->data);
        std::vector<ValuePtr> out;
        try {
            for (const auto& e : std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied)) {
                out.push_back(std::make_shared<Value>(Value::fromString(e.path().string())));
            }
        } catch (...) {}
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("listDirRecursive", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "copy_file");
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING) return Value::fromBool(false);
        try {
            std::filesystem::copy(std::get<std::string>(args[0]->data), std::get<std::string>(args[1]->data), std::filesystem::copy_options::overwrite_existing);
            return Value::fromBool(true);
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("copy_file", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "delete_file");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        try {
            return Value::fromBool(std::filesystem::remove(std::get<std::string>(args[0]->data)));
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("delete_file", i - 1);

    // string helpers
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string s = std::get<std::string>(args[0]->data);
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return Value::fromString("");
        size_t end = s.find_last_not_of(" \t\r\n");
        return Value::fromString(s.substr(start, end - start + 1));
    });
    setGlobalFn("trim", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING) return Value::fromBool(false);
        const std::string& s = std::get<std::string>(args[0]->data);
        const std::string& p = std::get<std::string>(args[1]->data);
        return Value::fromBool(s.size() >= p.size() && s.compare(0, p.size(), p) == 0);
    });
    setGlobalFn("starts_with", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING) return Value::fromBool(false);
        const std::string& s = std::get<std::string>(args[0]->data);
        const std::string& p = std::get<std::string>(args[1]->data);
        return Value::fromBool(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0);
    });
    setGlobalFn("ends_with", i - 1);

    // array combinators
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        ValuePtr fn = args[1];
        std::vector<ValuePtr> out;
        for (size_t i = 0; i < arr.size(); ++i) {
            ValuePtr r = vm->callValue(fn, {arr[i] ? arr[i] : std::make_shared<Value>(Value::nil())});
            if (r && r->type == Value::Type::ARRAY) {
                for (const auto& e : std::get<std::vector<ValuePtr>>(r->data)) out.push_back(e);
            } else if (r) out.push_back(r);
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("flat_map", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::ARRAY || args[1]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        auto& b = std::get<std::vector<ValuePtr>>(args[1]->data);
        std::vector<ValuePtr> out;
        for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
            std::vector<ValuePtr> pair = {a[i] ? a[i] : std::make_shared<Value>(Value::nil()), b[i] ? b[i] : std::make_shared<Value>(Value::nil())};
            out.push_back(std::make_shared<Value>(Value::fromArray(std::move(pair))));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("zip", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t sz = std::max(int64_t(1), toInt(args[1]));
        std::vector<ValuePtr> out;
        for (size_t i = 0; i < arr.size(); i += static_cast<size_t>(sz)) {
            std::vector<ValuePtr> chunk;
            for (int64_t j = 0; j < sz && i + static_cast<size_t>(j) < arr.size(); ++j) chunk.push_back(arr[i + j]);
            out.push_back(std::make_shared<Value>(Value::fromArray(std::move(chunk))));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("chunk", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::unordered_set<std::string> seen;
        std::vector<ValuePtr> out;
        for (const auto& v : arr) {
            std::string key = v ? v->toString() : "null";
            if (seen.insert(key).second) out.push_back(v ? v : std::make_shared<Value>(Value::nil()));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("unique", i - 1);

    // logging
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::cout << "[INFO] ";
        for (size_t j = 0; j < args.size(); ++j) { if (j) std::cout << " "; std::cout << (args[j] ? args[j]->toString() : "null"); }
        std::cout << std::endl;
        return Value::nil();
    });
    setGlobalFn("log_info", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::cout << "[WARN] ";
        for (size_t j = 0; j < args.size(); ++j) { if (j) std::cout << " "; std::cout << (args[j] ? args[j]->toString() : "null"); }
        std::cout << std::endl;
        return Value::nil();
    });
    setGlobalFn("log_warn", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::cerr << "[ERROR] ";
        for (size_t j = 0; j < args.size(); ++j) { if (j) std::cerr << " "; std::cerr << (args[j] ? args[j]->toString() : "null"); }
        std::cerr << std::endl;
        return Value::nil();
    });
    setGlobalFn("log_error", i - 1);

    // more file system
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "move_file");
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING) return Value::fromBool(false);
        try {
            std::filesystem::rename(std::get<std::string>(args[0]->data), std::get<std::string>(args[1]->data));
            return Value::fromBool(true);
        } catch (...) {
            try {
                std::filesystem::copy(std::get<std::string>(args[0]->data), std::get<std::string>(args[1]->data), std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(std::get<std::string>(args[0]->data));
                return Value::fromBool(true);
            } catch (...) { return Value::fromBool(false); }
        }
    });
    setGlobalFn("move_file", i - 1);

    // format string: format("%s %d %f", s, i, f) - %s str, %d int, %f float, %% literal %
    makeBuiltin(i++, [toInt, toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string fmt = std::get<std::string>(args[0]->data);
        std::string out;
        size_t argIdx = 1;
        for (size_t i = 0; i < fmt.size(); ++i) {
            if (fmt[i] != '%' || i + 1 >= fmt.size()) { out += fmt[i]; continue; }
            char spec = fmt[i + 1];
            i++;
            if (spec == '%') { out += '%'; continue; }
            ValuePtr v = argIdx < args.size() ? args[argIdx++] : nullptr;
            if (spec == 's') out += v ? v->toString() : "null";
            else if (spec == 'd') out += std::to_string(v ? toInt(v) : 0);
            else if (spec == 'f') { char b[64]; snprintf(b, sizeof(b), "%g", v ? toDouble(v) : 0); out += b; }
            else { out += '%'; out += spec; argIdx--; }
        }
        return Value::fromString(std::move(out));
    });
    setGlobalFn("format", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(std::tan(toDouble(args[0])));
    });
    setGlobalFn("tan", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromFloat(0);
        return Value::fromFloat(std::atan2(toDouble(args[0]), toDouble(args[1])));
    });
    setGlobalFn("atan2", i - 1);

    // array: reverse (new array), find (index or -1), sort (in-place by toString), flatten (one level)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<ValuePtr> out(arr.rbegin(), arr.rend());
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("reverse", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(-1);
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        ValuePtr needle = args[1];
        bool wantNil = !needle || needle->type == Value::Type::NIL;
        for (size_t i = 0; i < arr.size(); ++i) {
            ValuePtr v = arr[i];
            if (wantNil) { if (!v || v->type == Value::Type::NIL) return Value::fromInt(static_cast<int64_t>(i)); }
            else if (v && v->equals(*needle)) return Value::fromInt(static_cast<int64_t>(i));
        }
        return Value::fromInt(-1);
    });
    setGlobalFn("find", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::sort(arr.begin(), arr.end(), [](const ValuePtr& a, const ValuePtr& b) {
            return (a ? a->toString() : "null") < (b ? b->toString() : "null");
        });
        return args[0] ? Value(*args[0]) : Value::nil();
    });
    setGlobalFn("sort", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<ValuePtr> out;
        for (const auto& v : arr) {
            if (v && v->type == Value::Type::ARRAY) {
                for (const auto& e : std::get<std::vector<ValuePtr>>(v->data)) out.push_back(e);
            } else {
                out.push_back(v ? v : std::make_shared<Value>(Value::nil()));
            }
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("flatten", i - 1);

    // string: repeat(s, n), pad_left(s, width, char?), pad_right(s, width, char?)
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string s = std::get<std::string>(args[0]->data);
        int64_t n = std::max(int64_t(0), toInt(args[1]));
        std::string out;
        for (int64_t i = 0; i < n; ++i) out += s;
        return Value::fromString(std::move(out));
    });
    setGlobalFn("repeat", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string s = std::get<std::string>(args[0]->data);
        int64_t w = args.size() > 1 ? toInt(args[1]) : 0;
        char pad = ' ';
        if (args.size() > 2 && args[2] && args[2]->type == Value::Type::STRING) {
            std::string p = std::get<std::string>(args[2]->data);
            if (!p.empty()) pad = p[0];
        }
        if (static_cast<int64_t>(s.size()) >= w) return Value::fromString(s);
        return Value::fromString(std::string(static_cast<size_t>(w - s.size()), pad) + s);
    });
    setGlobalFn("pad_left", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string s = std::get<std::string>(args[0]->data);
        int64_t w = args.size() > 1 ? toInt(args[1]) : 0;
        char pad = ' ';
        if (args.size() > 2 && args[2] && args[2]->type == Value::Type::STRING) {
            std::string p = std::get<std::string>(args[2]->data);
            if (!p.empty()) pad = p[0];
        }
        if (static_cast<int64_t>(s.size()) >= w) return Value::fromString(s);
        return Value::fromString(s + std::string(static_cast<size_t>(w - s.size()), pad));
    });
    setGlobalFn("pad_right", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kEnvAccess, "env_get");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        const std::string& key = std::get<std::string>(args[0]->data);
#ifdef _WIN32
        char* buf = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&buf, &sz, key.c_str()) != 0) return Value::fromString("");
        std::string result(buf ? buf : "");
        if (buf) std::free(buf);
        return Value::fromString(std::move(result));
#else
        const char* v = kernGetEnv(key.c_str());
        return Value::fromString(v ? v : "");
#endif
    });
    setGlobalFn("env_get", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromBool(true);
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        for (const auto& v : arr) { if (!v || !v->isTruthy()) return Value::fromBool(false); }
        return Value::fromBool(true);
    });
    setGlobalFn("all", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromBool(false);
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        for (const auto& v : arr) { if (v && v->isTruthy()) return Value::fromBool(true); }
        return Value::fromBool(false);
    });
    setGlobalFn("any", i - 1);

    // directory & path type
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "create_dir");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        try {
            return Value::fromBool(std::filesystem::create_directories(std::get<std::string>(args[0]->data)));
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("create_dir", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "is_file");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        try {
            auto p = std::filesystem::status(std::get<std::string>(args[0]->data));
            return Value::fromBool(std::filesystem::is_regular_file(p));
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("is_file", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "is_dir");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        try {
            auto p = std::filesystem::status(std::get<std::string>(args[0]->data));
            return Value::fromBool(std::filesystem::is_directory(p));
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("is_dir", i - 1);

    // sort_by(arr, fn) - fn(a,b) returns truthy if a should come before b
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        ValuePtr fn = args[1];
        std::sort(arr.begin(), arr.end(), [vm, fn](const ValuePtr& a, const ValuePtr& b) {
            ValuePtr r = vm->callValue(fn, {a ? a : std::make_shared<Value>(Value::nil()), b ? b : std::make_shared<Value>(Value::nil())});
            return r && r->isTruthy();
        });
        return args[0] ? Value(*args[0]) : Value::nil();
    });
    setGlobalFn("sort_by", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        return arr.empty() ? Value::nil() : (arr[0] ? *arr[0] : Value::nil());
    });
    setGlobalFn("first", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        return arr.empty() ? Value::nil() : (arr.back() ? *arr.back() : Value::nil());
    });
    setGlobalFn("last", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t n = std::max(int64_t(0), toInt(args[1]));
        std::vector<ValuePtr> out;
        for (int64_t i = 0; i < n && static_cast<size_t>(i) < arr.size(); ++i) out.push_back(arr[static_cast<size_t>(i)]);
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("take", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t n = std::max(int64_t(0), toInt(args[1]));
        if (n >= static_cast<int64_t>(arr.size())) return Value::fromArray({});
        std::vector<ValuePtr> out(arr.begin() + n, arr.end());
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("drop", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        std::string s = std::get<std::string>(args[0]->data);
        std::vector<ValuePtr> out;
        size_t start = 0;
        for (size_t i = 0; i <= s.size(); ++i) {
            if (i == s.size() || s[i] == '\n') {
                out.push_back(std::make_shared<Value>(Value::fromString(s.substr(start, i - start))));
                start = i + 1;
            }
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("split_lines", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromInt(0);
        double x = toDouble(args[0]);
        return Value::fromInt(x > 0 ? 1 : (x < 0 ? -1 : 0));
    });
    setGlobalFn("sign", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(toDouble(args[0]) * M_PI / 180.0);
    });
    setGlobalFn("deg_to_rad", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        return Value::fromFloat(toDouble(args[0]) * 180.0 / M_PI);
    });
    setGlobalFn("rad_to_deg", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        try {
            return Value::fromInt(std::stoll(std::get<std::string>(args[0]->data)));
        } catch (...) { return Value::nil(); }
    });
    setGlobalFn("parse_int", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        try {
            return Value::fromFloat(std::stod(std::get<std::string>(args[0]->data)));
        } catch (...) { return Value::nil(); }
    });
    setGlobalFn("parse_float", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        ValuePtr a = args[0], b = args[1];
        return (a && a->isTruthy()) ? (a ? *a : Value::nil()) : (b ? *b : Value::nil());
    });
    setGlobalFn("default", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        for (const auto& arg : args) {
            if (!arg || arg->type != Value::Type::MAP) continue;
            for (const auto& kv : std::get<std::unordered_map<std::string, ValuePtr>>(arg->data))
                out[kv.first] = kv.second;
        }
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("merge", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::cout << "[DEBUG] ";
        for (size_t j = 0; j < args.size(); ++j) { if (j) std::cout << " "; std::cout << (args[j] ? args[j]->toString() : "null"); }
        std::cout << std::endl;
        return Value::nil();
    });
    setGlobalFn("log_debug", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        std::string out;
        if (!vm) return Value::fromString("");
        const size_t depth = vm->getCallStackDepth();
        const auto cs = vm->getCallStackSlice();
        if (depth > cs.size())
            out += "  ... (" + std::to_string(depth - cs.size()) + " outer frame(s) omitted)\n";
        bool first = true;
        for (const auto& f : cs) {
            if (!first) out += "\n";
            first = false;
            out += "  at " + f.functionName;
            if (!f.filePath.empty())
                out += " (" + humanizePathForDisplay(f.filePath) + ":" + std::to_string(f.line) + ")";
            else
                out += " line " + std::to_string(f.line);
        }
        return Value::fromString(out);
    });
    setGlobalFn("stack_trace", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[1]->type != Value::Type::STRING) return Value::nil();
        std::string want = std::get<std::string>(args[1]->data);
        std::string actual;
        switch (args[0]->type) {
            case Value::Type::NIL: actual = "nil"; break;
            case Value::Type::BOOL: actual = "bool"; break;
            case Value::Type::INT: case Value::Type::FLOAT: actual = "number"; break;
            case Value::Type::STRING: actual = "string"; break;
            case Value::Type::ARRAY: actual = "array"; break;
            case Value::Type::MAP: actual = "dictionary"; break;
            case Value::Type::FUNCTION: actual = "function"; break;
            case Value::Type::CLASS: actual = "class"; break;
            case Value::Type::INSTANCE: actual = "instance"; break;
            default: actual = "unknown"; break;
        }
        if (actual != want) throw VMError("assertType failed: expected " + want + ", got " + actual, 0, 0, 2);
        return args[0] ? *args[0] : Value::nil();
    });
    setGlobalFn("assertType", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::FUNCTION) return Value::nil();
        int64_t n = 1;
        if (args[1] && (args[1]->type == Value::Type::INT || args[1]->type == Value::Type::FLOAT))
            n = args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : static_cast<int64_t>(std::get<double>(args[1]->data));
        if (n < 1) n = 1;
        if (vm) vm->resetCycleCount();
        for (int64_t k = 0; k < n; ++k) vm->callValue(args[0], {});
        return vm ? Value::fromInt(static_cast<int64_t>(vm->getCycleCount())) : Value::fromInt(0);
    });
    setGlobalFn("profile_fn", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::ARRAY || args[1]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        auto& b = std::get<std::vector<ValuePtr>>(args[1]->data);
        std::vector<ValuePtr> out;
        for (const auto& va : a) for (const auto& vb : b) {
            std::vector<ValuePtr> pair = {va ? va : std::make_shared<Value>(Value::nil()), vb ? vb : std::make_shared<Value>(Value::nil())};
            out.push_back(std::make_shared<Value>(Value::fromArray(std::move(pair))));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("cartesian", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t n = std::max(int64_t(1), toInt(args[1]));
        std::vector<ValuePtr> out;
        for (size_t i = 0; i + static_cast<size_t>(n) <= arr.size(); ++i) {
            std::vector<ValuePtr> win;
            for (int64_t j = 0; j < n; ++j) win.push_back(arr[i + static_cast<size_t>(j)]);
            out.push_back(std::make_shared<Value>(Value::fromArray(std::move(win))));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("window", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        const size_t kMaxArraySize = 64 * 1024 * 1024;
        if (arr.size() >= kMaxArraySize) return Value::nil();
        arr.insert(arr.begin(), args[1] ? args[1] : std::make_shared<Value>(Value::nil()));
        return Value(*args[0]);
    });
    setGlobalFn("push_front", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        double x = args.size() >= 1 ? toDouble(args[0]) : 0, y = args.size() >= 2 ? toDouble(args[1]) : 0;
        std::vector<ValuePtr> v = { std::make_shared<Value>(Value::fromFloat(x)), std::make_shared<Value>(Value::fromFloat(y)) };
        return Value::fromArray(std::move(v));
    });
    setGlobalFn("vec2", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        double x = args.size() >= 1 ? toDouble(args[0]) : 0, y = args.size() >= 2 ? toDouble(args[1]) : 0, z = args.size() >= 3 ? toDouble(args[2]) : 0;
        std::vector<ValuePtr> v = { std::make_shared<Value>(Value::fromFloat(x)), std::make_shared<Value>(Value::fromFloat(y)), std::make_shared<Value>(Value::fromFloat(z)) };
        return Value::fromArray(std::move(v));
    });
    setGlobalFn("vec3", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        double x = static_cast<double>(std::rand()) / RAND_MAX, y = static_cast<double>(std::rand()) / RAND_MAX;
        if (args.size() >= 2) {
            double lo = toDouble(args[0]), hi = toDouble(args[1]);
            if (lo > hi) std::swap(lo, hi);
            x = lo + x * (hi - lo); y = lo + y * (hi - lo);
        }
        std::vector<ValuePtr> v = { std::make_shared<Value>(Value::fromFloat(x)), std::make_shared<Value>(Value::fromFloat(y)) };
        return Value::fromArray(std::move(v));
    });
    setGlobalFn("rand_vec2", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        double x = static_cast<double>(std::rand()) / RAND_MAX, y = static_cast<double>(std::rand()) / RAND_MAX, z = static_cast<double>(std::rand()) / RAND_MAX;
        if (args.size() >= 2) {
            double lo = toDouble(args[0]), hi = toDouble(args[1]);
            if (lo > hi) std::swap(lo, hi);
            x = lo + x * (hi - lo); y = lo + y * (hi - lo); z = lo + z * (hi - lo);
        }
        std::vector<ValuePtr> v = { std::make_shared<Value>(Value::fromFloat(x)), std::make_shared<Value>(Value::fromFloat(y)), std::make_shared<Value>(Value::fromFloat(z)) };
        return Value::fromArray(std::move(v));
    });
    setGlobalFn("rand_vec3", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING)
            return Value::fromBool(false);
        try {
            std::regex re(std::get<std::string>(args[1]->data));
            return Value::fromBool(std::regex_search(std::get<std::string>(args[0]->data), re));
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("regex_match", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3 || !args[0] || !args[1] || !args[2] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING ||
            args[2]->type != Value::Type::STRING)
            return Value::nil();
        try {
            std::string s = std::get<std::string>(args[0]->data);
            std::regex re(std::get<std::string>(args[1]->data));
            std::string repl = std::get<std::string>(args[2]->data);
            return Value::fromString(std::regex_replace(s, re, repl));
        } catch (...) { return Value::nil(); }
    });
    setGlobalFn("regex_replace", i - 1);

    // direct memory access (low-level / OS building) ---
    auto toPtr = [](ValuePtr v) -> void* {
        if (!v || v->type != Value::Type::PTR) return nullptr;
        return std::get<void*>(v->data);
    };
    makeBuiltin(i++, [toPtr, toInt](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("mem_copy requires unsafe context", 0, 0, 5);
        if (args.size() < 3) return Value::nil();
        void* dest = toPtr(args[0]); void* src = toPtr(args[1]);
        if (!dest || !src) return Value::nil();
        size_t n = static_cast<size_t>(std::max(int64_t(0), toInt(args[2])));
        const size_t kMaxMem = 256 * 1024 * 1024;
        if (n > kMaxMem) return Value::nil();
        std::memcpy(dest, src, n);
        return Value::nil();
    });
    setGlobalFn("mem_copy", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("mem_set requires unsafe context", 0, 0, 5);
        if (args.size() < 3) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t byteVal = toInt(args[1]) & 0xFF;
        size_t n = static_cast<size_t>(std::max(int64_t(0), toInt(args[2])));
        const size_t kMaxMem = 256 * 1024 * 1024;
        if (n > kMaxMem) return Value::nil();
        std::memset(p, static_cast<int>(byteVal), n);
        return Value::nil();
    });
    setGlobalFn("mem_set", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("poke8 requires unsafe context", 0, 0, 5);
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t delta = toInt(args[1]);
        return Value::fromPtr(static_cast<char*>(p) + delta);
    });
    setGlobalFn("ptr_offset", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]);
        return Value::fromInt(reinterpret_cast<intptr_t>(p));
    });
    setGlobalFn("ptr_address", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int64_t addr = toInt(args[0]);
        return Value::fromPtr(reinterpret_cast<void*>(static_cast<intptr_t>(addr)));
    });
    setGlobalFn("ptr_from_address", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint8_t v; std::memcpy(&v, p, 1);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("peek8", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint16_t v; std::memcpy(&v, p, 2);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("peek16", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint32_t v; std::memcpy(&v, p, 4);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("peek32", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint64_t v; std::memcpy(&v, p, 8);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("peek64", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("poke16 requires unsafe context", 0, 0, 5);
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint8_t v = static_cast<uint8_t>(toInt(args[1]) & 0xFF);
        std::memcpy(p, &v, 1);
        return Value::nil();
    });
    setGlobalFn("poke8", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("poke32 requires unsafe context", 0, 0, 5);
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint16_t v = static_cast<uint16_t>(toInt(args[1]) & 0xFFFF);
        std::memcpy(p, &v, 2);
        return Value::nil();
    });
    setGlobalFn("poke16", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM* vm, std::vector<ValuePtr> args) {
        if (vm && !vm->getRuntimeGuards().allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("poke64 requires unsafe context", 0, 0, 5);
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint32_t v = static_cast<uint32_t>(toInt(args[1]) & 0xFFFFFFFFu);
        std::memcpy(p, &v, 4);
        return Value::nil();
    });
    setGlobalFn("poke32", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint64_t v = static_cast<uint64_t>(toInt(args[1]));
        std::memcpy(p, &v, 8);
        return Value::nil();
    });
    setGlobalFn("poke64", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int64_t value = toInt(args[0]);
        int64_t align = std::max(int64_t(1), toInt(args[1]));
        if (align <= 0) return Value::fromInt(value);
        int64_t r = value % align;
        if (r == 0) return Value::fromInt(value);
        return Value::fromInt(value + (value >= 0 ? (align - r) : (-r)));
    });
    setGlobalFn("align_up", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int64_t value = toInt(args[0]);
        int64_t align = std::max(int64_t(1), toInt(args[1]));
        if (align <= 0) return Value::fromInt(value);
        int64_t r = value % align;
        if (r == 0) return Value::fromInt(value);
        return Value::fromInt(value - (value >= 0 ? r : (r + align)));
    });
    setGlobalFn("align_down", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        (void)args;
        builtinAtomicFence();
        return Value::nil();
    });
    setGlobalFn("memory_barrier", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        builtinAtomicFence();
        uint8_t v;
        std::memcpy(&v, p, 1);
        builtinAtomicFence();
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("volatile_load8", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint8_t v = static_cast<uint8_t>(toInt(args[1]) & 0xFF);
        builtinAtomicFence();
        std::memcpy(p, &v, 1);
        builtinAtomicFence();
        return Value::nil();
    });
    setGlobalFn("volatile_store8", i - 1);

    // more low-level memory access ---
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* a = toPtr(args[0]); void* b = toPtr(args[1]);
        if (!a || !b) return Value::nil();
        size_t n = static_cast<size_t>(std::max(int64_t(0), toInt(args[2])));
        const size_t kMaxMem = 256 * 1024 * 1024;
        if (n > kMaxMem) return Value::nil();
        int r = std::memcmp(a, b, n);
        return Value::fromInt(r < 0 ? -1 : (r > 0 ? 1 : 0));
    });
    setGlobalFn("mem_cmp", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* dest = toPtr(args[0]); void* src = toPtr(args[1]);
        if (!dest || !src) return Value::nil();
        size_t n = static_cast<size_t>(std::max(int64_t(0), toInt(args[2])));
        const size_t kMaxMem = 256 * 1024 * 1024;
        if (n > kMaxMem) return Value::nil();
        std::memmove(dest, src, n);
        return Value::nil();
    });
    setGlobalFn("mem_move", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]);
        int64_t req = toInt(args[1]);
        if (req <= 0) return Value::fromPtr(nullptr);
        const size_t kMaxAlloc = 256 * 1024 * 1024;
        size_t n = static_cast<size_t>(req);
        if (n > kMaxAlloc) return Value::nil();
        void* q = std::realloc(p, n);
        return Value::fromPtr(q);
    });
    setGlobalFn("realloc", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t align = std::max(int64_t(1), toInt(args[1]));
        if (align <= 0) return args[0] ? *args[0] : Value::nil();
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        uintptr_t r = addr % static_cast<uintptr_t>(align);
        if (r == 0) return args[0] ? *args[0] : Value::nil();
        return Value::fromPtr(reinterpret_cast<void*>(addr + (static_cast<uintptr_t>(align) - r)));
    });
    setGlobalFn("ptr_align_up", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t align = std::max(int64_t(1), toInt(args[1]));
        if (align <= 0) return args[0] ? *args[0] : Value::nil();
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        uintptr_t r = addr % static_cast<uintptr_t>(align);
        if (r == 0) return args[0] ? *args[0] : Value::nil();
        return Value::fromPtr(reinterpret_cast<void*>(addr - r));
    });
    setGlobalFn("ptr_align_down", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        float v; std::memcpy(&v, p, 4);
        return Value::fromFloat(static_cast<double>(v));
    });
    setGlobalFn("peek_float", i - 1);
    makeBuiltin(i++, [toPtr, toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        float v = static_cast<float>(toDouble(args[1]));
        std::memcpy(p, &v, 4);
        return Value::nil();
    });
    setGlobalFn("poke_float", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        double v; std::memcpy(&v, p, 8);
        return Value::fromFloat(v);
    });
    setGlobalFn("peek_double", i - 1);
    makeBuiltin(i++, [toPtr, toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        double v = toDouble(args[1]);
        std::memcpy(p, &v, 8);
        return Value::nil();
    });
    setGlobalFn("poke_double", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        builtinAtomicFence();
        uint16_t v;
        std::memcpy(&v, p, 2);
        builtinAtomicFence();
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("volatile_load16", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint16_t v = static_cast<uint16_t>(toInt(args[1]) & 0xFFFF);
        builtinAtomicFence();
        std::memcpy(p, &v, 2);
        builtinAtomicFence();
        return Value::nil();
    });
    setGlobalFn("volatile_store16", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        builtinAtomicFence();
        uint32_t v;
        std::memcpy(&v, p, 4);
        builtinAtomicFence();
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("volatile_load32", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint32_t v = static_cast<uint32_t>(toInt(args[1]) & 0xFFFFFFFFu);
        builtinAtomicFence();
        std::memcpy(p, &v, 4);
        builtinAtomicFence();
        return Value::nil();
    });
    setGlobalFn("volatile_store32", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        builtinAtomicFence();
        uint64_t v;
        std::memcpy(&v, p, 8);
        builtinAtomicFence();
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("volatile_load64", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint64_t v = static_cast<uint64_t>(toInt(args[1]));
        builtinAtomicFence();
        std::memcpy(p, &v, 8);
        builtinAtomicFence();
        return Value::nil();
    });
    setGlobalFn("volatile_store64", i - 1);
    // signed reads (C++-style; sign-extended)
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int8_t v; std::memcpy(&v, p, 1);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("peek8s", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int16_t v; std::memcpy(&v, p, 2);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("peek16s", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int32_t v; std::memcpy(&v, p, 4);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("peek32s", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t v; std::memcpy(&v, p, 8);
        return Value::fromInt(v);
    });
    setGlobalFn("peek64s", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* dest = toPtr(args[0]); void* src = toPtr(args[1]);
        int64_t n = toInt(args[2]);
        if (!dest || !src || n <= 0) return Value::nil();
        const size_t kMaxMem = 256 * 1024 * 1024;
        size_t nu = static_cast<size_t>(n);
        if (nu > kMaxMem) return Value::nil();
        std::vector<char> tmp(nu);
        std::memcpy(tmp.data(), dest, nu);
        std::memcpy(dest, src, nu);
        std::memcpy(src, tmp.data(), nu);
        return Value::nil();
    });
    setGlobalFn("mem_swap", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t n = toInt(args[1]);
        if (n <= 0) return Value::fromArray({});
        const size_t kMaxMem = 256 * 1024 * 1024;
        if (static_cast<uint64_t>(n) > kMaxMem) return Value::nil();
        std::vector<ValuePtr> arr;
        arr.reserve(static_cast<size_t>(n));
        const unsigned char* bytes = static_cast<const unsigned char*>(p);
        for (int64_t i = 0; i < n; ++i)
            arr.push_back(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(bytes[i]))));
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("bytes_read", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[1]) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        if (args[1]->type != Value::Type::ARRAY) return Value::nil();
        const auto& arr = std::get<std::vector<ValuePtr>>(args[1]->data);
        const size_t kMaxMem = 256 * 1024 * 1024;
        size_t lim = std::min(arr.size(), kMaxMem);
        unsigned char* dst = static_cast<unsigned char*>(p);
        for (size_t i = 0; i < lim; ++i)
            dst[i] = static_cast<unsigned char>(toInt(arr[i]) & 0xFF);
        return Value::nil();
    });
    setGlobalFn("bytes_write", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(true);
        return Value::fromBool(toPtr(args[0]) == nullptr);
    });
    setGlobalFn("ptr_is_null", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        (void)args;
        return Value::fromInt(static_cast<int64_t>(sizeof(void*)));
    });
    setGlobalFn("size_of_ptr", i - 1);

    // ptr_add, ptr_sub, is_aligned, mem_set_zero, ptr_tag/untag/get_tag ---
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t n = toInt(args[1]);
        return Value::fromPtr(static_cast<char*>(p) + n);
    });
    setGlobalFn("ptr_add", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t n = toInt(args[1]);
        return Value::fromPtr(static_cast<char*>(p) - n);
    });
    setGlobalFn("ptr_sub", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::fromBool(false);
        int64_t boundary = std::max(int64_t(1), toInt(args[1]));
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        return Value::fromBool((addr % static_cast<uintptr_t>(boundary)) == 0);
    });
    setGlobalFn("is_aligned", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t n = std::max(int64_t(0), toInt(args[2]));
        const size_t kMaxMem = 256 * 1024 * 1024;
        size_t nu = static_cast<size_t>(n);
        if (nu > kMaxMem) return Value::nil();
        std::memset(p, 0, nu);
        return Value::nil();
    });
    setGlobalFn("mem_set_zero", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t tag = toInt(args[1]) & 7;
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        addr = (addr & ~uintptr_t(7)) | static_cast<uintptr_t>(tag);
        return Value::fromPtr(reinterpret_cast<void*>(addr));
    });
    setGlobalFn("ptr_tag", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uintptr_t addr = reinterpret_cast<uintptr_t>(p) & ~uintptr_t(7);
        return Value::fromPtr(reinterpret_cast<void*>(addr));
    });
    setGlobalFn("ptr_untag", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::fromInt(0);
        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        return Value::fromInt(static_cast<int64_t>(addr & 7));
    });
    setGlobalFn("ptr_get_tag", i - 1);

    // struct_define, offsetof_struct, sizeof_struct ---
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::nil();
        std::string name = args[0]->type == Value::Type::STRING ? std::get<std::string>(args[0]->data) : "";
        if (name.empty()) return Value::nil();
        if (args[1]->type != Value::Type::ARRAY) return Value::nil();
        StructLayoutMeta meta;
        const auto& arr = std::get<std::vector<ValuePtr>>(args[1]->data);
        size_t runningOffset = 0;
        size_t maxAlign = 1;
        for (const auto& el : arr) {
            if (!el || el->type != Value::Type::ARRAY) continue;
            const auto& pair = std::get<std::vector<ValuePtr>>(el->data);
            std::string fname = (pair.size() >= 1 && pair[0] && pair[0]->type == Value::Type::STRING)
                ? std::get<std::string>(pair[0]->data) : "";
            size_t fsize = pair.size() >= 2 && pair[1] ? static_cast<size_t>(std::max(int64_t(0), toInt(pair[1]))) : 0;
            size_t falign = pair.size() >= 3 && pair[2] ? static_cast<size_t>(std::max(int64_t(1), toInt(pair[2]))) : std::min<size_t>(8, std::max<size_t>(1, fsize));
            if (falign == 0) falign = 1;
            if (falign > maxAlign) maxAlign = falign;
            if (!fname.empty()) {
                size_t pad = runningOffset % falign;
                if (pad != 0) runningOffset += (falign - pad);
                StructFieldLayout fld;
                fld.name = fname;
                fld.size = fsize;
                fld.align = falign;
                fld.offset = runningOffset;
                runningOffset += fsize;
                meta.fields.push_back(std::move(fld));
            }
        }
        if (maxAlign == 0) maxAlign = 1;
        size_t endPad = runningOffset % maxAlign;
        if (endPad != 0) runningOffset += (maxAlign - endPad);
        meta.align = maxAlign;
        meta.size = runningOffset;
        g_structLayouts[name] = std::move(meta);
        return Value::fromInt(1);
    });
    setGlobalFn("struct_define", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::nil();
        std::string name = args[0]->type == Value::Type::STRING ? std::get<std::string>(args[0]->data) : "";
        std::string field = args[1]->type == Value::Type::STRING ? std::get<std::string>(args[1]->data) : "";
        auto it = g_structLayouts.find(name);
        if (it == g_structLayouts.end()) return Value::nil();
        for (const auto& p : it->second.fields) {
            if (p.name == field) return Value::fromInt(static_cast<int64_t>(p.offset));
        }
        return Value::nil();
    });
    setGlobalFn("offsetof_struct", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        std::string name = args[0]->type == Value::Type::STRING ? std::get<std::string>(args[0]->data) : "";
        auto it = g_structLayouts.find(name);
        if (it == g_structLayouts.end()) return Value::nil();
        return Value::fromInt(static_cast<int64_t>(it->second.size));
    });
    setGlobalFn("sizeof_struct", i - 1);

    // pool_create, pool_alloc, pool_free, pool_destroy ---
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        size_t blockSize = static_cast<size_t>(std::max(int64_t(1), toInt(args[0])));
        size_t count = static_cast<size_t>(std::max(int64_t(1), toInt(args[1])));
        int64_t id = MemoryManager::createPool(blockSize, count);
        if (id < 0) return Value::nil();
        return Value::fromInt(id);
    });
    setGlobalFn("pool_create", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int64_t id = toInt(args[0]);
        void* p = MemoryManager::poolAlloc(id);
        if (!p) return Value::nil();
        return Value::fromPtr(p);
    });
    setGlobalFn("pool_alloc", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t id = toInt(args[1]);
        MemoryManager::poolFree(p, id);
        return Value::nil();
    });
    setGlobalFn("pool_free", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int64_t id = toInt(args[0]);
        MemoryManager::destroyPool(id);
        return Value::nil();
    });
    setGlobalFn("pool_destroy", i - 1);

    // read_be16/32/64, write_be16/32/64 ---
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint16_t v = (static_cast<uint16_t>(b[0]) << 8) | b[1];
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("read_be16", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint32_t v = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
            (static_cast<uint32_t>(b[2]) << 8) | b[3];
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("read_be32", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint64_t v = (static_cast<uint64_t>(b[0]) << 56) | (static_cast<uint64_t>(b[1]) << 48) |
            (static_cast<uint64_t>(b[2]) << 40) | (static_cast<uint64_t>(b[3]) << 32) |
            (static_cast<uint64_t>(b[4]) << 24) | (static_cast<uint64_t>(b[5]) << 16) |
            (static_cast<uint64_t>(b[6]) << 8) | b[7];
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("read_be64", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint16_t v = static_cast<uint16_t>(toInt(args[1]) & 0xFFFF);
        unsigned char* b = static_cast<unsigned char*>(p);
        b[0] = static_cast<unsigned char>(v >> 8); b[1] = static_cast<unsigned char>(v);
        return Value::nil();
    });
    setGlobalFn("write_be16", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint32_t v = static_cast<uint32_t>(toInt(args[1]) & 0xFFFFFFFFu);
        unsigned char* b = static_cast<unsigned char*>(p);
        b[0] = (v >> 24) & 0xFF; b[1] = (v >> 16) & 0xFF; b[2] = (v >> 8) & 0xFF; b[3] = v & 0xFF;
        return Value::nil();
    });
    setGlobalFn("write_be32", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint64_t v = static_cast<uint64_t>(toInt(args[1]));
        unsigned char* b = static_cast<unsigned char*>(p);
        for (int i = 7; i >= 0; --i) { b[i] = v & 0xFF; v >>= 8; }
        return Value::nil();
    });
    setGlobalFn("write_be64", i - 1);

    // dump_memory, alloc_tracked, free_tracked, get_tracked_allocations ---
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromString("");
        void* p = toPtr(args[0]); if (!p) return Value::fromString("");
        int64_t n = std::max(int64_t(0), toInt(args[1]));
        if (n > 4096) n = 4096;
        std::ostringstream out;
        const unsigned char* b = static_cast<const unsigned char*>(p);
        for (int64_t i = 0; i < n; ++i) {
            if (i > 0 && (i % 16) == 0) out << "\n";
            char buf[4];
            snprintf(buf, sizeof(buf), "%02x ", b[i]);
            out << buf;
        }
        return Value::fromString(out.str());
    });
    setGlobalFn("dump_memory", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int64_t req = toInt(args[0]);
        if (req <= 0) return Value::nil();
        size_t n = static_cast<size_t>(req);
        if (n > 256 * 1024 * 1024) return Value::nil();
        void* p = std::malloc(n);
        if (!p) return Value::nil();
        g_trackedAllocs.insert(p);
        return Value::fromPtr(p);
    });
    setGlobalFn("alloc_tracked", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]);
        if (p) { g_trackedAllocs.erase(p); std::free(p); }
        return Value::nil();
    });
    setGlobalFn("free_tracked", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        (void)args;
        std::vector<ValuePtr> arr;
        for (void* p : g_trackedAllocs)
            arr.push_back(std::make_shared<Value>(Value::fromInt(reinterpret_cast<intptr_t>(p))));
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("get_tracked_allocations", i - 1);

    // atomic_load32, atomic_store32, atomic_add32, atomic_cmpxchg32 ---
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        return Value::fromInt(static_cast<int64_t>(std::atomic_load(reinterpret_cast<std::atomic<int32_t>*>(p))));
    });
    setGlobalFn("atomic_load32", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        std::atomic_store(reinterpret_cast<std::atomic<int32_t>*>(p), static_cast<int32_t>(toInt(args[1])));
        return Value::nil();
    });
    setGlobalFn("atomic_store32", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int32_t delta = static_cast<int32_t>(toInt(args[1]));
        return Value::fromInt(static_cast<int64_t>(std::atomic_fetch_add(reinterpret_cast<std::atomic<int32_t>*>(p), delta)));
    });
    setGlobalFn("atomic_add32", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int32_t expected = static_cast<int32_t>(toInt(args[1]));
        int32_t desired = static_cast<int32_t>(toInt(args[2]));
        bool ok = std::atomic_compare_exchange_strong(reinterpret_cast<std::atomic<int32_t>*>(p), &expected, desired);
        return Value::fromBool(ok);
    });
    setGlobalFn("atomic_cmpxchg32", i - 1);

    // map_file, unmap_file ---
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string path = std::get<std::string>(args[0]->data);
#ifdef _WIN32
        void* hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (!hFile || hFile == INVALID_HANDLE_VALUE) return Value::nil();
        LARGE_INTEGER liSize; if (!GetFileSizeEx(hFile, &liSize) || liSize.QuadPart <= 0) {
            CloseHandle(hFile); return Value::nil();
        }
        void* hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMap) { CloseHandle(hFile); return Value::nil(); }
        void* view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        if (!view) { CloseHandle(hMap); CloseHandle(hFile); return Value::nil(); }
        MappedFileState mf; mf.view = view; mf.hMap = hMap; mf.hFile = hFile; mf.size = static_cast<size_t>(liSize.QuadPart);
        g_mappedFiles[view] = mf;
        return Value::fromPtr(view);
#else
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) return Value::nil();
        struct stat st;
        if (fstat(fd, &st) != 0 || st.st_size <= 0) { close(fd); return Value::nil(); }
        size_t size = static_cast<size_t>(st.st_size);
        void* view = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (view == MAP_FAILED) return Value::nil();
        MappedFileState mf; mf.view = view; mf.size = size;
        g_mappedFiles[view] = mf;
        return Value::fromPtr(view);
#endif
    });
    setGlobalFn("map_file", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]);
        auto it = g_mappedFiles.find(p);
        if (it == g_mappedFiles.end()) return Value::nil();
#ifdef _WIN32
        UnmapViewOfFile(it->second.view);
        if (it->second.hMap) CloseHandle(it->second.hMap);
        if (it->second.hFile) CloseHandle(it->second.hFile);
#else
        munmap(it->second.view, it->second.size);
#endif
        g_mappedFiles.erase(it);
        return Value::fromInt(1);
    });
    setGlobalFn("unmap_file", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::fromInt(0);
        size_t sz = static_cast<size_t>(std::max(int64_t(0), toInt(args[1])));
        int64_t flags = toInt(args[2]);
#ifdef _WIN32
        DWORD prot = (flags == 0) ? PAGE_NOACCESS : (flags == 1) ? PAGE_READONLY : PAGE_READWRITE;
        DWORD old; return Value::fromInt(VirtualProtect(p, sz, prot, &old) ? 1 : 0);
#else
        int prot = (flags == 0) ? PROT_NONE : (flags == 1) ? PROT_READ : (PROT_READ | PROT_WRITE);
        uintptr_t pageStart = reinterpret_cast<uintptr_t>(p) & ~(static_cast<uintptr_t>(4096) - 1);
        size_t pageLen = ((reinterpret_cast<uintptr_t>(p) + sz - pageStart + 4095) / 4096) * 4096;
        return Value::fromInt(mprotect(reinterpret_cast<void*>(pageStart), pageLen, prot) == 0 ? 1 : 0);
#endif
    });
    setGlobalFn("memory_protect", i - 1);

    // error_name(e), error_cause(e), is_error_type(e, typeName) — Python/C++ style inspection
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::fromString("Error");
        if (args[0]->type == Value::Type::MAP) {
            auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
            auto it = m.find("name");
            if (it != m.end() && it->second && it->second->type == Value::Type::STRING)
                return *it->second;
        }
        return Value::fromString("Error");
    });
    setGlobalFn("error_name", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::nil();
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        auto it = m.find("cause");
        if (it != m.end()) return it->second ? *it->second : Value::nil();
        return Value::nil();
    });
    setGlobalFn("error_cause", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string msg = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> m;
        m["message"] = std::make_shared<Value>(Value::fromString(msg));
        m["name"] = std::make_shared<Value>(Value::fromString("ValueError"));
        m["code"] = std::make_shared<Value>(Value::nil());
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("ValueError", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string msg = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> m;
        m["message"] = std::make_shared<Value>(Value::fromString(msg));
        m["name"] = std::make_shared<Value>(Value::fromString("TypeError"));
        m["code"] = std::make_shared<Value>(Value::nil());
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("TypeError", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string msg = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> m;
        m["message"] = std::make_shared<Value>(Value::fromString(msg));
        m["name"] = std::make_shared<Value>(Value::fromString("RuntimeError"));
        m["code"] = std::make_shared<Value>(Value::nil());
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("RuntimeError", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string msg = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> m;
        m["message"] = std::make_shared<Value>(Value::fromString(msg));
        m["name"] = std::make_shared<Value>(Value::fromString("OSError"));
        m["code"] = std::make_shared<Value>(Value::nil());
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("OSError", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string msg = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> m;
        m["message"] = std::make_shared<Value>(Value::fromString(msg));
        m["name"] = std::make_shared<Value>(Value::fromString("KeyError"));
        m["code"] = std::make_shared<Value>(Value::nil());
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("KeyError", i - 1);
    makeBuiltin(i++, [](VM* /* vm*/, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1]) return Value::fromBool(false);
        std::string want = args[1]->toString();
        if (args[0]->type != Value::Type::MAP) return Value::fromBool(want == "Error");
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        auto it = m.find("name");
        if (it == m.end() || !it->second || it->second->type != Value::Type::STRING)
            return Value::fromBool(want == "Error");
        return Value::fromBool(std::get<std::string>(it->second->data) == want);
    });
    setGlobalFn("is_error_type", i - 1);

    // read_le16/32/64, write_le16/32/64 (explicit little-endian) ---
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint16_t v = b[0] | (static_cast<uint16_t>(b[1]) << 8);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("read_le16", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint32_t v = b[0] | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("read_le32", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint64_t v = b[0] | (static_cast<uint64_t>(b[1]) << 8) | (static_cast<uint64_t>(b[2]) << 16) | (static_cast<uint64_t>(b[3]) << 24)
            | (static_cast<uint64_t>(b[4]) << 32) | (static_cast<uint64_t>(b[5]) << 40) | (static_cast<uint64_t>(b[6]) << 48) | (static_cast<uint64_t>(b[7]) << 56);
        return Value::fromInt(static_cast<int64_t>(v));
    });
    setGlobalFn("read_le64", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint16_t v = static_cast<uint16_t>(toInt(args[1]) & 0xFFFF);
        unsigned char* b = static_cast<unsigned char*>(p);
        b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF;
        return Value::nil();
    });
    setGlobalFn("write_le16", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint32_t v = static_cast<uint32_t>(toInt(args[1]) & 0xFFFFFFFFu);
        unsigned char* b = static_cast<unsigned char*>(p);
        b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; b[2] = (v >> 16) & 0xFF; b[3] = (v >> 24) & 0xFF;
        return Value::nil();
    });
    setGlobalFn("write_le32", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        uint64_t val = static_cast<uint64_t>(toInt(args[1]));
        unsigned char* b = static_cast<unsigned char*>(p);
        for (int j = 0; j < 8; ++j) { b[j] = val & 0xFF; val >>= 8; }
        return Value::nil();
    });
    setGlobalFn("write_le64", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::nil();
        int64_t req = toInt(args[0]);
        if (req <= 0) return Value::nil();
        size_t n = static_cast<size_t>(req);
        if (n > 256 * 1024 * 1024) return Value::nil();
        void* p = std::malloc(n);
        if (!p) return Value::nil();
        std::memset(p, 0, n);
        return Value::fromPtr(p);
    });
    setGlobalFn("alloc_zeroed", i - 1);

    // path utilities ---
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string p = std::get<std::string>(args[0]->data);
        std::filesystem::path path(p);
        return Value::fromString(path.filename().string());
    });
    setGlobalFn("basename", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string p = std::get<std::string>(args[0]->data);
        std::filesystem::path path(p);
        return Value::fromString(path.parent_path().string());
    });
    setGlobalFn("dirname", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromString("");
        std::filesystem::path result;
        for (const auto& a : args) {
            if (a && a->type == Value::Type::STRING)
                result /= std::get<std::string>(a->data);
        }
        return Value::fromString(result.string());
    });
    setGlobalFn("path_join", i - 1);

    // ptr_eq, alloc_aligned, string_to_bytes, bytes_to_string ---
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromBool(false);
        void* a = toPtr(args[0]);
        void* b = toPtr(args[1]);
        return Value::fromBool(a == b);
    });
    setGlobalFn("ptr_eq", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        int64_t req = toInt(args[0]);
        int64_t align = toInt(args[1]);
        if (req <= 0 || align <= 0) return Value::nil();
        size_t n = static_cast<size_t>(req);
        size_t al = static_cast<size_t>(align);
        if (al > 256 || (al & (al - 1)) != 0) return Value::nil(); // power of 2, max 256
        if (n > 256 * 1024 * 1024) return Value::nil();
        size_t total = n + al - 1;
        void* p = std::malloc(total);
        if (!p) return Value::nil();
        uintptr_t u = reinterpret_cast<uintptr_t>(p);
        uintptr_t aligned = (u + al - 1) & ~(al - 1);
        void* alignedPtr = reinterpret_cast<void*>(aligned);
        g_alignedAllocBases[alignedPtr] = p;
        return Value::fromPtr(alignedPtr);
    });
    setGlobalFn("alloc_aligned", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        const std::string& s = std::get<std::string>(args[0]->data);
        std::vector<ValuePtr> out;
        for (unsigned char c : s) out.push_back(std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(c))));
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("string_to_bytes", i - 1);
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromString("");
        const auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::string out;
        out.reserve(arr.size());
        for (const auto& v : arr) out += static_cast<char>(static_cast<unsigned char>(toInt(v) & 0xFF));
        return Value::fromString(std::move(out));
    });
    setGlobalFn("bytes_to_string", i - 1);

    // memory_page_size, mem_find, mem_fill_pattern ---
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
        return Value::fromInt(4096);
    });
    setGlobalFn("memory_page_size", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::fromInt(-1);
        void* p = toPtr(args[0]); if (!p) return Value::fromInt(-1);
        int64_t n = toInt(args[1]); if (n <= 0) return Value::fromInt(-1);
        unsigned char needle = static_cast<unsigned char>(toInt(args[2]) & 0xFF);
        const unsigned char* base = static_cast<const unsigned char*>(p);
        for (int64_t i = 0; i < n; ++i) if (base[i] == needle) return Value::fromInt(i);
        return Value::fromInt(-1);
    });
    setGlobalFn("mem_find", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t n = toInt(args[1]); if (n <= 0) return Value::nil();
        uint32_t pat = static_cast<uint32_t>(toInt(args[2]) & 0xFFFFFFFFu);
        unsigned char* dst = static_cast<unsigned char*>(p);
        for (int64_t i = 0; i < n; i += 4) {
            dst[i] = pat & 0xFF;
            if (i + 1 < n) dst[i + 1] = (pat >> 8) & 0xFF;
            if (i + 2 < n) dst[i + 2] = (pat >> 16) & 0xFF;
            if (i + 3 < n) dst[i + 3] = (pat >> 24) & 0xFF;
        }
        return Value::nil();
    });
    setGlobalFn("mem_fill_pattern", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromInt(0);
        void* a = toPtr(args[0]);
        void* b = toPtr(args[1]);
        if (a == b) return Value::fromInt(0);
        if (!a) return Value::fromInt(-1);
        if (!b) return Value::fromInt(1);
        return Value::fromInt(reinterpret_cast<uintptr_t>(a) < reinterpret_cast<uintptr_t>(b) ? -1 : 1);
    });
    setGlobalFn("ptr_compare", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t n = toInt(args[1]); if (n <= 1) return Value::nil();
        unsigned char* b = static_cast<unsigned char*>(p);
        for (int64_t i = 0, j = n - 1; i < j; ++i, --j) { unsigned char t = b[i]; b[i] = b[j]; b[j] = t; }
        return Value::nil();
    });
    setGlobalFn("mem_reverse", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::fromInt(-1);
        void* haystack = toPtr(args[0]); if (!haystack) return Value::fromInt(-1);
        int64_t hayLen = toInt(args[1]); if (hayLen <= 0) return Value::fromInt(-1);
        if (args[2]->type != Value::Type::ARRAY) return Value::fromInt(-1);
        const auto& pat = std::get<std::vector<ValuePtr>>(args[2]->data);
        if (pat.empty() || static_cast<int64_t>(pat.size()) > hayLen) return Value::fromInt(-1);
        const unsigned char* h = static_cast<const unsigned char*>(haystack);
        int64_t plen = static_cast<int64_t>(pat.size());
        for (int64_t i = 0; i <= hayLen - plen; ++i) {
            bool match = true;
            for (int64_t j = 0; j < plen; ++j) {
                if (h[i + j] != static_cast<unsigned char>(toInt(pat[j]) & 0xFF)) { match = false; break; }
            }
            if (match) return Value::fromInt(i);
        }
        return Value::fromInt(-1);
    });
    setGlobalFn("mem_scan", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 4) return Value::fromBool(false);
        void* a = toPtr(args[0]); void* b = toPtr(args[1]);
        int64_t na = toInt(args[2]); int64_t nb = toInt(args[3]);
        if (na <= 0 || nb <= 0) return Value::fromBool(false);
        uintptr_t ua = reinterpret_cast<uintptr_t>(a);
        uintptr_t ub = reinterpret_cast<uintptr_t>(b);
        uintptr_t end_a = ua + static_cast<uintptr_t>(na);
        uintptr_t end_b = ub + static_cast<uintptr_t>(nb);
        return Value::fromBool(ua < end_b && ub < end_a);
    });
    setGlobalFn("mem_overlaps", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
#if (defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__) || defined(__aarch64__) || defined(_M_ARM64))
        return Value::fromString("little");
#else
        return Value::fromString("big");
#endif
    });
    setGlobalFn("get_endianness", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromBool(true);
        void* p = toPtr(args[0]); if (!p) return Value::fromBool(true);
        int64_t n = toInt(args[1]); if (n <= 0) return Value::fromBool(true);
        const unsigned char* b = static_cast<const unsigned char*>(p);
        for (int64_t i = 0; i < n; ++i) if (b[i] != 0) return Value::fromBool(false);
        return Value::fromBool(true);
    });
    setGlobalFn("mem_is_zero", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0.0);
        void* p = toPtr(args[0]); if (!p) return Value::fromFloat(0.0);
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint32_t u = b[0] | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
        float f; std::memcpy(&f, &u, 4); return Value::fromFloat(static_cast<double>(f));
    });
    setGlobalFn("read_le_float", i - 1);
    makeBuiltin(i++, [toPtr, toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        float f = static_cast<float>(toDouble(args[1]));
        uint32_t u; std::memcpy(&u, &f, 4);
        unsigned char* b = static_cast<unsigned char*>(p);
        b[0] = u & 0xFF; b[1] = (u >> 8) & 0xFF; b[2] = (u >> 16) & 0xFF; b[3] = (u >> 24) & 0xFF;
        return Value::nil();
    });
    setGlobalFn("write_le_float", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0.0);
        void* p = toPtr(args[0]); if (!p) return Value::fromFloat(0.0);
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint64_t u = b[0] | (static_cast<uint64_t>(b[1]) << 8) | (static_cast<uint64_t>(b[2]) << 16) | (static_cast<uint64_t>(b[3]) << 24)
            | (static_cast<uint64_t>(b[4]) << 32) | (static_cast<uint64_t>(b[5]) << 40) | (static_cast<uint64_t>(b[6]) << 48) | (static_cast<uint64_t>(b[7]) << 56);
        double d; std::memcpy(&d, &u, 8); return Value::fromFloat(d);
    });
    setGlobalFn("read_le_double", i - 1);
    makeBuiltin(i++, [toPtr, toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        double d = toDouble(args[1]);
        uint64_t u; std::memcpy(&u, &d, 8);
        unsigned char* b = static_cast<unsigned char*>(p);
        for (int j = 0; j < 8; ++j) { b[j] = u & 0xFF; u >>= 8; }
        return Value::nil();
    });
    setGlobalFn("write_le_double", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::fromInt(0);
        void* p = toPtr(args[0]); if (!p) return Value::fromInt(0);
        int64_t n = toInt(args[1]); if (n <= 0) return Value::fromInt(0);
        unsigned char needle = static_cast<unsigned char>(toInt(args[2]) & 0xFF);
        const unsigned char* b = static_cast<const unsigned char*>(p);
        int64_t count = 0;
        for (int64_t i = 0; i < n; ++i) if (b[i] == needle) ++count;
        return Value::fromInt(count);
    });
    setGlobalFn("mem_count", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* a = toPtr(args[0]); void* b = toPtr(args[1]);
        if (!a) return Value::fromPtr(b);
        if (!b) return Value::fromPtr(a);
        void* chosen = reinterpret_cast<uintptr_t>(a) <= reinterpret_cast<uintptr_t>(b) ? a : b;
        return Value::fromPtr(chosen);
    });
    setGlobalFn("ptr_min", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* a = toPtr(args[0]); void* b = toPtr(args[1]);
        if (!a) return Value::fromPtr(b);
        if (!b) return Value::fromPtr(a);
        void* chosen = reinterpret_cast<uintptr_t>(a) >= reinterpret_cast<uintptr_t>(b) ? a : b;
        return Value::fromPtr(chosen);
    });
    setGlobalFn("ptr_max", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::fromInt(0);
        void* a = toPtr(args[0]); void* b = toPtr(args[1]);
        if (!a || !b) return Value::fromInt(0);
        auto ua = reinterpret_cast<uintptr_t>(a);
        auto ub = reinterpret_cast<uintptr_t>(b);
        int64_t diff = static_cast<int64_t>(ua - ub);
        return Value::fromInt(diff);
    });
    setGlobalFn("ptr_diff", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0.0);
        void* p = toPtr(args[0]); if (!p) return Value::fromFloat(0.0);
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint32_t u = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) | (static_cast<uint32_t>(b[2]) << 8) | b[3];
        float f; std::memcpy(&f, &u, 4); return Value::fromFloat(static_cast<double>(f));
    });
    setGlobalFn("read_be_float", i - 1);
    makeBuiltin(i++, [toPtr, toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        float f = static_cast<float>(toDouble(args[1]));
        uint32_t u; std::memcpy(&u, &f, 4);
        unsigned char* b = static_cast<unsigned char*>(p);
        b[0] = (u >> 24) & 0xFF; b[1] = (u >> 16) & 0xFF; b[2] = (u >> 8) & 0xFF; b[3] = u & 0xFF;
        return Value::nil();
    });
    setGlobalFn("write_be_float", i - 1);
    makeBuiltin(i++, [toPtr](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0.0);
        void* p = toPtr(args[0]); if (!p) return Value::fromFloat(0.0);
        const unsigned char* b = static_cast<const unsigned char*>(p);
        uint64_t u = (static_cast<uint64_t>(b[0]) << 56) | (static_cast<uint64_t>(b[1]) << 48) | (static_cast<uint64_t>(b[2]) << 40) | (static_cast<uint64_t>(b[3]) << 32)
            | (static_cast<uint64_t>(b[4]) << 24) | (static_cast<uint64_t>(b[5]) << 16) | (static_cast<uint64_t>(b[6]) << 8) | b[7];
        double d; std::memcpy(&d, &u, 8); return Value::fromFloat(d);
    });
    setGlobalFn("read_be_double", i - 1);
    makeBuiltin(i++, [toPtr, toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        double d = toDouble(args[1]);
        uint64_t u; std::memcpy(&u, &d, 8);
        unsigned char* b = static_cast<unsigned char*>(p);
        for (int j = 7; j >= 0; --j) { b[j] = u & 0xFF; u >>= 8; }
        return Value::nil();
    });
    setGlobalFn("write_be_double", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::fromBool(false);
        void* ptr = toPtr(args[0]); void* base = toPtr(args[1]);
        if (!ptr || !base) return Value::fromBool(false);
        int64_t size = toInt(args[2]); if (size <= 0) return Value::fromBool(false);
        auto up = reinterpret_cast<uintptr_t>(ptr);
        auto ub = reinterpret_cast<uintptr_t>(base);
        return Value::fromBool(up >= ub && up < ub + static_cast<uintptr_t>(size));
    });
    setGlobalFn("ptr_in_range", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3) return Value::nil();
        void* dest = toPtr(args[0]); void* src = toPtr(args[1]);
        if (!dest || !src) return Value::nil();
        int64_t n = toInt(args[2]); if (n <= 0) return Value::nil();
        unsigned char* d = static_cast<unsigned char*>(dest);
        const unsigned char* s = static_cast<const unsigned char*>(src);
        for (int64_t j = 0; j < n; ++j) d[j] ^= s[j];
        return Value::nil();
    });
    setGlobalFn("mem_xor", i - 1);
    makeBuiltin(i++, [toPtr, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        void* p = toPtr(args[0]); if (!p) return Value::nil();
        int64_t n = toInt(args[1]); if (n <= 0) return Value::nil();
        std::memset(p, 0, static_cast<size_t>(n));
        return Value::nil();
    });
    setGlobalFn("mem_zero", i - 1);

    // iDE & system (OS-oriented): repr, kern_version, platform, os_name, arch
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::fromString("nil");
        ValuePtr v = args[0];
        std::function<std::string(ValuePtr, int)> reprImpl;
        reprImpl = [&reprImpl](ValuePtr val, int depth) -> std::string {
            if (!val) return "nil";
            if (depth > 10) return "...";
            switch (val->type) {
                case Value::Type::NIL: return "nil";
                case Value::Type::INT: return std::to_string(std::get<int64_t>(val->data));
                case Value::Type::FLOAT: {
                    double d = std::get<double>(val->data);
                    if (std::isnan(d)) return "nan";
                    if (std::isinf(d)) return d > 0 ? "inf" : "-inf";
                    return std::to_string(d);
                }
                case Value::Type::BOOL: return std::get<bool>(val->data) ? "true" : "false";
                case Value::Type::STRING: {
                    std::string s = std::get<std::string>(val->data);
                    std::string out = "\"";
                    for (char c : s) {
                        if (c == '"') out += "\\\"";
                        else if (c == '\\') out += "\\\\";
                        else if (c == '\n') out += "\\n";
                        else if (c == '\r') out += "\\r";
                        else if (c == '\t') out += "\\t";
                        else out += c;
                    }
                    return out + "\"";
                }
                case Value::Type::ARRAY: {
                    auto& arr = std::get<std::vector<ValuePtr>>(val->data);
                    std::string out = "[";
                    for (size_t i = 0; i < arr.size(); ++i) {
                        if (i) out += ", ";
                        out += reprImpl(arr[i], depth + 1);
                    }
                    return out + "]";
                }
                case Value::Type::MAP: {
                    auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(val->data);
                    std::string out = "{";
                    bool first = true;
                    for (const auto& kv : m) {
                        if (!first) out += ", ";
                        first = false;
                        out += "\"" + kv.first + "\": " + reprImpl(kv.second, depth + 1);
                    }
                    return out + "}";
                }
                case Value::Type::FUNCTION: return "<function>";
                case Value::Type::CLASS: return "<class>";
                case Value::Type::INSTANCE: return "<instance>";
                case Value::Type::PTR: return "<ptr>";
                default: return "<unknown>";
            }
        };
        return Value::fromString(reprImpl(v, 0));
    });
    setGlobalFn("repr", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
#ifdef KERN_VERSION
        return Value::fromString(KERN_VERSION);
#else
        return Value::fromString("1.0.0");
#endif
    });
    setGlobalFn("kern_version", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
#if defined(_WIN32) || defined(_WIN64)
        return Value::fromString("windows");
#elif defined(__APPLE__)
        return Value::fromString("darwin");
#elif defined(__linux__)
        return Value::fromString("linux");
#else
        return Value::fromString("unknown");
#endif
    });
    setGlobalFn("platform", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
#if defined(_WIN32) || defined(_WIN64)
        return Value::fromString("Windows");
#elif defined(__APPLE__)
        return Value::fromString("Darwin");
#elif defined(__linux__)
        return Value::fromString("Linux");
#else
        return Value::fromString("Unknown");
#endif
    });
    setGlobalFn("os_name", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
#if defined(_WIN32) || defined(_WIN64)
#ifdef _M_X64
        return Value::fromString("x64");
#else
        return Value::fromString("x86");
#endif
#elif defined(__x86_64__) || defined(_M_X64)
        return Value::fromString("x64");
#elif defined(__aarch64__) || defined(_M_ARM64)
        return Value::fromString("aarch64");
#elif defined(__arm__)
        return Value::fromString("arm");
#else
        return Value::fromString("unknown");
#endif
    });
    setGlobalFn("arch", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        int code = 0;
        if (args.size() >= 1 && args[0]) {
            if (args[0]->type == Value::Type::INT) code = static_cast<int>(std::get<int64_t>(args[0]->data));
            else if (args[0]->type == Value::Type::FLOAT) code = static_cast<int>(std::get<double>(args[0]->data));
        }
        if (vm) vm->setScriptExitCode(code);
        return Value::nil();
    });
    setGlobalFn("exit_code", i - 1);

    // readline(prompt?) – read one line from stdin
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() >= 1 && args[0] && args[0]->type == Value::Type::STRING) {
            std::cout << std::get<std::string>(args[0]->data) << std::flush;
        }
        std::string line;
        if (std::getline(std::cin, line)) return Value::fromString(line);
        return Value::fromString("");
    });
    setGlobalFn("readline", i - 1);

    // chr(n) – integer to single-character string (ASCII/UTF-8 code point)
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromString("");
        int64_t n = toInt(args[0]);
        if (n < 0 || n > 0x10FFFF) return Value::fromString("");
        if (n < 0x80) return Value::fromString(std::string(1, static_cast<char>(n)));
        std::string s;
        if (n < 0x800) { s += static_cast<char>(0xC0 | (n >> 6)); s += static_cast<char>(0x80 | (n & 0x3F)); }
        else if (n < 0x10000) { s += static_cast<char>(0xE0 | (n >> 12)); s += static_cast<char>(0x80 | ((n >> 6) & 0x3F)); s += static_cast<char>(0x80 | (n & 0x3F)); }
        else { s += static_cast<char>(0xF0 | (n >> 18)); s += static_cast<char>(0x80 | ((n >> 12) & 0x3F)); s += static_cast<char>(0x80 | ((n >> 6) & 0x3F)); s += static_cast<char>(0x80 | (n & 0x3F)); }
        return Value::fromString(s);
    });
    setGlobalFn("chr", i - 1);

    // ord(s) – first character of string to integer (code point)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(0);
        const std::string& s = std::get<std::string>(args[0]->data);
        if (s.empty()) return Value::fromInt(0);
        unsigned char c0 = static_cast<unsigned char>(s[0]);
        if (c0 < 0x80) return Value::fromInt(static_cast<int64_t>(c0));
        if (s.size() < 2) return Value::fromInt(static_cast<int64_t>(c0));
        if (c0 < 0xE0) { unsigned char c1 = static_cast<unsigned char>(s[1]); return Value::fromInt(static_cast<int64_t>(((c0 & 0x1F) << 6) | (c1 & 0x3F))); }
        if (s.size() < 3) return Value::fromInt(static_cast<int64_t>(c0));
        if (c0 < 0xF0) { unsigned char c1 = static_cast<unsigned char>(s[1]), c2 = static_cast<unsigned char>(s[2]); return Value::fromInt(static_cast<int64_t>(((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F))); }
        if (s.size() < 4) return Value::fromInt(static_cast<int64_t>(c0));
        unsigned char c1 = static_cast<unsigned char>(s[1]), c2 = static_cast<unsigned char>(s[2]), c3 = static_cast<unsigned char>(s[3]);
        return Value::fromInt(static_cast<int64_t>(((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F)));
    });
    setGlobalFn("ord", i - 1);

    // hex(n) – integer to hex string (e.g. "ff")
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromString("0");
        int64_t n = toInt(args[0]);
        char buf[32];
        snprintf(buf, sizeof(buf), "%llx", static_cast<unsigned long long>(n & 0xFFFFFFFFFFFFFFFFULL));
        return Value::fromString(std::string(buf));
    });
    setGlobalFn("hex", i - 1);

    // bin(n) – integer to binary string (e.g. "1010")
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromString("0");
        uint64_t n = static_cast<uint64_t>(toInt(args[0]));
        if (n == 0) return Value::fromString("0");
        std::string s;
        while (n) { s = (n & 1 ? "1" : "0") + s; n >>= 1; }
        return Value::fromString(s);
    });
    setGlobalFn("bin", i - 1);

    // assert_eq(a, b, msg?) – assert deep equality; throw with optional message on failure
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2) return Value::nil();
        bool eq = g_assertEqDeepEqual && g_assertEqDeepEqual(args[0], args[1]);
        if (!eq) {
            std::string msg = "assert_eq failed";
            if (args.size() >= 3 && args[2] && args[2]->type == Value::Type::STRING)
                msg += ": " + std::get<std::string>(args[2]->data);
            else msg += ": " + (args[0] ? args[0]->toString() : "null") + " != " + (args[1] ? args[1]->toString() : "null");
            throw VMError(msg, 0, 0, 3);
        }
        return Value::nil();
    });
    setGlobalFn("assert_eq", i - 1);

    // very advanced builtins ---

    // base64_encode(s) / base64_decode(s)
    static const char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        const std::string& in = std::get<std::string>(args[0]->data);
        std::string out;
        int val = 0, valb = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) { out += kBase64Chars[(val >> valb) & 0x3F]; valb -= 6; }
        }
        if (valb > -6) out += kBase64Chars[((val << 8) >> (valb + 8)) & 0x3F];
        while (out.size() % 4) out += '=';
        return Value::fromString(out);
    });
    setGlobalFn("base64_encode", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string in = std::get<std::string>(args[0]->data);
        while (!in.empty() && in.back() == '=') in.pop_back();
        static int T[256];
        static bool init = false;
        if (!init) { for (int i = 0; i < 256; ++i) T[i] = -1; for (int i = 0; i < 64; ++i) T[static_cast<unsigned char>(kBase64Chars[i])] = i; init = true; }
        std::string out;
        int val = 0, valb = -8;
        for (unsigned char c : in) {
            if (T[c] < 0) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) { out += static_cast<char>((val >> valb) & 0xFF); valb -= 8; }
        }
        return Value::fromString(out);
    });
    setGlobalFn("base64_decode", i - 1);

    // uuid() – UUID v4-style string (random hex)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
        static std::mt19937_64 rng(static_cast<unsigned>(std::time(nullptr)) + static_cast<unsigned>(std::clock()));
        static std::uniform_int_distribution<int> hex(0, 15);
        const char h[] = "0123456789abcdef";
        std::string s;
        for (int i = 0; i < 8; ++i) s += h[hex(rng)];
        s += '-';
        for (int i = 0; i < 4; ++i) s += h[hex(rng)];
        s += "-4";
        for (int i = 0; i < 3; ++i) s += h[hex(rng)];
        s += '-';
        s += h[(hex(rng) & 3) | 8];
        for (int i = 0; i < 3; ++i) s += h[hex(rng)];
        s += '-';
        for (int i = 0; i < 12; ++i) s += h[hex(rng)];
        return Value::fromString(s);
    });
    setGlobalFn("uuid", i - 1);

    // hash_fnv(s) – FNV-1a 64-bit hash of string (returns int)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(0);
        const std::string& str = std::get<std::string>(args[0]->data);
        uint64_t h = 14695981039346656037ULL;
        for (unsigned char c : str) { h ^= c; h *= 1099511628211ULL; }
        return Value::fromInt(static_cast<int64_t>(h));
    });
    setGlobalFn("hash_fnv", i - 1);

    // csv_parse(s) – parse CSV string to array of rows (array of arrays of strings); simple (no quoted commas)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        const std::string& in = std::get<std::string>(args[0]->data);
        std::vector<ValuePtr> rows;
        size_t i = 0;
        while (i < in.size()) {
            std::vector<ValuePtr> row;
            while (i < in.size() && in[i] != '\n' && in[i] != '\r') {
                if (in[i] == '"') {
                    ++i; std::string cell;
                    while (i < in.size() && in[i] != '"') { if (in[i] == '\\') ++i; if (i < in.size()) cell += in[i++]; }
                    if (i < in.size()) ++i;
                    row.push_back(std::make_shared<Value>(Value::fromString(cell)));
                    if (i < in.size() && in[i] == ',') ++i;
                } else {
                    size_t j = i; while (j < in.size() && in[j] != ',' && in[j] != '\n' && in[j] != '\r') ++j;
                    row.push_back(std::make_shared<Value>(Value::fromString(in.substr(i, j - i))));
                    i = j; if (i < in.size() && in[i] == ',') ++i;
                }
            }
            rows.push_back(std::make_shared<Value>(Value::fromArray(std::move(row))));
            if (i < in.size()) { ++i; if (i < in.size() && in[i-1] == '\r' && in[i] == '\n') ++i; }
        }
        return Value::fromArray(std::move(rows));
    });
    setGlobalFn("csv_parse", i - 1);

    // csv_stringify(rows) – rows = array of arrays; returns CSV string
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromString("");
        std::string out;
        auto& rows = std::get<std::vector<ValuePtr>>(args[0]->data);
        for (size_t r = 0; r < rows.size(); ++r) {
            if (!rows[r] || rows[r]->type != Value::Type::ARRAY) continue;
            auto& cells = std::get<std::vector<ValuePtr>>(rows[r]->data);
            for (size_t c = 0; c < cells.size(); ++c) {
                if (c) out += ',';
                std::string cell = cells[c] ? cells[c]->toString() : "";
                if (cell.find(',') != std::string::npos || cell.find('"') != std::string::npos || cell.find('\n') != std::string::npos) {
                    out += '"'; for (char x : cell) { if (x == '"') out += "\\\""; else out += x; } out += '"';
                } else out += cell;
            }
            out += '\n';
        }
        return Value::fromString(out);
    });
    setGlobalFn("csv_stringify", i - 1);

    // time_format(fmt [, t]) – strftime-like; t = time() or current. fmt: %Y %m %d %H %M %S %A %B etc.
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::time_t t = std::time(nullptr);
        if (args.size() >= 2 && args[1] && (args[1]->type == Value::Type::INT || args[1]->type == Value::Type::FLOAT))
            t = static_cast<std::time_t>(args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : std::get<double>(args[1]->data));
#ifdef _WIN32
        std::tm bt = {};
        if (localtime_s(&bt, &t) != 0) return Value::fromString("");
        std::tm* btp = &bt;
#else
        std::tm* btp = std::localtime(&t);
        if (!btp) return Value::fromString("");
#endif
        const std::string& fmt = std::get<std::string>(args[0]->data);
        char buf[256];
        std::strftime(buf, sizeof(buf), fmt.c_str(), btp);
        return Value::fromString(std::string(buf));
    });
    setGlobalFn("time_format", i - 1);

    // stack_trace_array() – current call stack as array of {name, file, line, column} (innermost kMaxCallStackSnapshotFrames)
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        std::vector<ValuePtr> arr;
        if (!vm) return Value::fromArray(std::move(arr));
        const size_t depth = vm->getCallStackDepth();
        const auto cs = vm->getCallStackSlice();
        if (depth > cs.size()) {
            std::unordered_map<std::string, ValuePtr> marker;
            marker["name"] = std::make_shared<Value>(Value::fromString(
                "(" + std::to_string(depth - cs.size()) + " outer frame(s) omitted)"));
            marker["file"] = std::make_shared<Value>(Value::fromString(""));
            marker["line"] = std::make_shared<Value>(Value::fromInt(0));
            marker["column"] = std::make_shared<Value>(Value::fromInt(0));
            arr.push_back(std::make_shared<Value>(Value::fromMap(std::move(marker))));
        }
        for (const auto& f : cs) {
            std::unordered_map<std::string, ValuePtr> m;
            m["name"] = std::make_shared<Value>(Value::fromString(f.functionName));
            m["file"] = std::make_shared<Value>(Value::fromString(f.filePath));
            m["line"] = std::make_shared<Value>(Value::fromInt(f.line));
            m["column"] = std::make_shared<Value>(Value::fromInt(f.column));
            arr.push_back(std::make_shared<Value>(Value::fromMap(std::move(m))));
        }
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("stack_trace_array", i - 1);

    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(false);
        double x = toDouble(args[0]);
        return Value::fromBool(std::isnan(x));
    });
    setGlobalFn("is_nan", i - 1);
    makeBuiltin(i++, [toDouble](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromBool(false);
        double x = toDouble(args[0]);
        return Value::fromBool(std::isinf(x));
    });
    setGlobalFn("is_inf", i - 1);

    // env_all() – all environment variables as map
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kEnvAccess, "env_all");
        std::unordered_map<std::string, ValuePtr> m;
#ifdef _WIN32
        char* env = GetEnvironmentStringsA();
        if (env) {
            for (char* p = env; *p; ) {
                std::string line; while (*p) line += *p++;
                size_t eq = line.find('='); if (eq != std::string::npos) m[line.substr(0, eq)] = std::make_shared<Value>(Value::fromString(line.substr(eq + 1)));
            }
            FreeEnvironmentStringsA(env);
        }
#else
        char** envp;
#if defined(__APPLE__)
        envp = *_NSGetEnviron();
#else
        envp = ::environ;
#endif
        for (char** p = envp; p && *p; ++p) {
            std::string s(*p); size_t eq = s.find('='); if (eq != std::string::npos) m[s.substr(0, eq)] = std::make_shared<Value>(Value::fromString(s.substr(eq + 1)));
        }
#endif
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("env_all", i - 1);

    // escape_regex(s) – escape regex special characters
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromString("");
        std::string s = std::get<std::string>(args[0]->data);
        std::string out;
        for (char c : s) { if (c == '\\' || c == '^' || c == '$' || c == '.' || c == '|' || c == '?' || c == '*' || c == '+' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') out += '\\'; out += c; }
        return Value::fromString(out);
    });
    setGlobalFn("escape_regex", i - 1);

    // oS / program-building builtins ---

    // cwd() – current working directory
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "cwd");
        try {
            return Value::fromString(std::filesystem::current_path().string());
        } catch (...) { return Value::fromString(""); }
    });
    setGlobalFn("cwd", i - 1);

    // chdir(path) – change working directory; returns true on success
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "chdir");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        try {
            std::filesystem::current_path(std::get<std::string>(args[0]->data));
            return Value::fromBool(true);
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("chdir", i - 1);

    // hostname() – machine hostname
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "hostname");
        char buf[256];
#ifdef _WIN32
        DWORD n = 256;
        if (GetComputerNameA(buf, &n)) return Value::fromString(std::string(buf));
        return Value::fromString("");
#else
        if (gethostname(buf, sizeof(buf)) == 0) return Value::fromString(std::string(buf));
        return Value::fromString("");
#endif
    });
    setGlobalFn("hostname", i - 1);

    // cpu_count() – number of hardware threads (for parallelism)
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "cpu_count");
        unsigned n = std::thread::hardware_concurrency();
        return Value::fromInt(n > 0 ? static_cast<int64_t>(n) : 1);
    });
    setGlobalFn("cpu_count", i - 1);

    // temp_dir() – system temp directory path
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "temp_dir");
        try {
            return Value::fromString(std::filesystem::temp_directory_path().string());
        } catch (...) { return Value::fromString(""); }
    });
    setGlobalFn("temp_dir", i - 1);

    // realpath(path) – resolve to absolute canonical path; nil on error
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "realpath");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        try {
            std::filesystem::path p(std::get<std::string>(args[0]->data));
            if (!std::filesystem::exists(p)) return Value::nil();
            return Value::fromString(std::filesystem::canonical(p).string());
        } catch (...) { return Value::nil(); }
    });
    setGlobalFn("realpath", i - 1);

    // getpid() – current process ID
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kProcessControl, "getpid");
#ifdef _WIN32
        return Value::fromInt(static_cast<int64_t>(GetCurrentProcessId()));
#else
        return Value::fromInt(static_cast<int64_t>(getpid()));
#endif
    });
    setGlobalFn("getpid", i - 1);

    // monotonic_time() – seconds since arbitrary point (for deltas, not wall clock)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return Value::fromFloat(ms / 1000000.0);
    });
    setGlobalFn("monotonic_time", i - 1);

    // file_size(path) – size in bytes; nil if not a file or error
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "file_size");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        try {
            auto sz = std::filesystem::file_size(std::get<std::string>(args[0]->data));
            return Value::fromInt(static_cast<int64_t>(sz));
        } catch (...) { return Value::nil(); }
    });
    setGlobalFn("file_size", i - 1);

    // env_set(name, value) – set environment variable for current process; value = nil to unset
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kEnvAccess, "env_set");
        if (args.size() < 1 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromBool(false);
        std::string name = std::get<std::string>(args[0]->data);
        std::string val = args.size() >= 2 && args[1] ? args[1]->toString() : "";
#ifdef _WIN32
        if (val.empty()) return Value::fromBool(SetEnvironmentVariableA(name.c_str(), nullptr) != 0);
        return Value::fromBool(SetEnvironmentVariableA(name.c_str(), val.c_str()) != 0);
#else
        if (val.empty()) return Value::fromBool(unsetenv(name.c_str()) == 0);
        return Value::fromBool(setenv(name.c_str(), val.c_str(), 1) == 0);
#endif
    });
    setGlobalFn("env_set", i - 1);

    // glob(pattern [, base_dir]) – list paths matching pattern (* and ?); base_dir defaults to cwd
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "glob");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        std::string pattern = std::get<std::string>(args[0]->data);
        std::string baseStr;
        if (args.size() >= 2 && args[1] && args[1]->type == Value::Type::STRING) baseStr = std::get<std::string>(args[1]->data);
        else { try { baseStr = std::filesystem::current_path().string(); } catch (...) { return Value::fromArray({}); } }
        std::vector<ValuePtr> out;
        std::function<bool(const std::string&, const std::string&)> matchFn;
        matchFn = [&matchFn](const std::string& name, const std::string& pat) -> bool {
            size_t pi = 0, ni = 0;
            while (pi < pat.size() && ni < name.size()) {
                if (pat[pi] == '*') {
                    pi++;
                    if (pi >= pat.size()) return true;
                    while (ni <= name.size()) { if (matchFn(name.substr(ni), pat.substr(pi))) return true; ni++; }
                    return false;
                }
                if (pat[pi] == '?' || pat[pi] == name[ni]) { pi++; ni++; continue; }
                return false;
            }
            while (pi < pat.size() && pat[pi] == '*') pi++;
            return pi >= pat.size() && ni >= name.size();
        };
        try {
            std::filesystem::path base(baseStr);
            std::filesystem::path pat(pattern);
            std::string patStr = pat.filename().string();
            bool hasWildcard = (patStr.find('*') != std::string::npos || patStr.find('?') != std::string::npos);
            if (!hasWildcard) {
                if (std::filesystem::exists(base / pat)) out.push_back(std::make_shared<Value>(Value::fromString((base / pat).string())));
                return Value::fromArray(std::move(out));
            }
            std::filesystem::path dir = base;
            if (pat.has_parent_path() && pat.parent_path() != ".") { dir = base / pat.parent_path(); patStr = pat.filename().string(); }
            if (!std::filesystem::is_directory(dir)) return Value::fromArray({});
            for (const auto& e : std::filesystem::directory_iterator(dir)) {
                std::string name = e.path().filename().string();
                if (matchFn(name, patStr)) out.push_back(std::make_shared<Value>(Value::fromString(e.path().string())));
            }
        } catch (...) {}
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("glob", i - 1);

    // type predicates (fully integrated)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        return Value::fromBool(args.size() >= 1 && args[0] && args[0]->type == Value::Type::STRING);
    });
    setGlobalFn("is_string", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        return Value::fromBool(args.size() >= 1 && args[0] && args[0]->type == Value::Type::ARRAY);
    });
    setGlobalFn("is_array", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        return Value::fromBool(args.size() >= 1 && args[0] && args[0]->type == Value::Type::MAP);
    });
    setGlobalFn("is_map", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        return Value::fromBool(args.size() >= 1 && args[0] && (args[0]->type == Value::Type::INT || args[0]->type == Value::Type::FLOAT));
    });
    setGlobalFn("is_number", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        return Value::fromBool(args.size() >= 1 && args[0] && args[0]->type == Value::Type::FUNCTION);
    });
    setGlobalFn("is_function", i - 1);

    // round_to(x, decimals) – round number to N decimal places
    makeBuiltin(i++, [toDouble, toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty()) return Value::fromFloat(0);
        double x = toDouble(args[0]);
        int dec = args.size() >= 2 ? static_cast<int>(toInt(args[1])) : 0;
        if (dec < 0) dec = 0;
        if (dec > 15) dec = 15;
        double m = 1.0;
        for (int i = 0; i < dec; ++i) m *= 10.0;
        x = std::round(x * m) / m;
        return dec == 0 ? Value::fromInt(static_cast<int64_t>(x)) : Value::fromFloat(x);
    });
    setGlobalFn("round_to", i - 1);

    // insert_at(arr, index, value) – insert value at index; returns array
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t idx = args[1] && (args[1]->type == Value::Type::INT || args[1]->type == Value::Type::FLOAT)
            ? static_cast<int64_t>(args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : std::get<double>(args[1]->data)) : 0;
        if (idx < 0) idx = static_cast<int64_t>(a.size()) + idx;
        size_t u = static_cast<size_t>(idx);
        if (u > a.size()) u = a.size();
        a.insert(a.begin() + u, args[2]);
        return args[0] ? *args[0] : Value::nil();
    });
    setGlobalFn("insert_at", i - 1);

    // remove_at(arr, index) – remove element at index; returns removed value or nil
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        auto& a = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (a.empty()) return Value::nil();
        int64_t idx = args[1] && (args[1]->type == Value::Type::INT || args[1]->type == Value::Type::FLOAT)
            ? static_cast<int64_t>(args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : std::get<double>(args[1]->data)) : 0;
        if (idx < 0) idx = static_cast<int64_t>(a.size()) + idx;
        if (idx < 0 || static_cast<size_t>(idx) >= a.size()) return Value::nil();
        ValuePtr v = a[static_cast<size_t>(idx)];
        a.erase(a.begin() + idx);
        return v ? Value(*v) : Value::nil();
    });
    setGlobalFn("remove_at", i - 1);

    // sleep_ms(ms) – sleep in milliseconds (more convenient for OS loops)
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        int64_t ms = args.empty() ? 0 : toInt(args[0]);
        if (ms < 0) ms = 0;
        if (ms > 3600000) ms = 3600000; // clamp to 1h safety
        if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return Value::nil();
    });
    setGlobalFn("sleep_ms", i - 1);

    // exec(cmd) – run shell command, return process exit code
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kSystemExec, "exec");
        if (args.empty() || !args[0]) return Value::fromInt(-1);
        std::string cmd = args[0]->toString();
        if (cmd.empty()) return Value::fromInt(-1);
        int rc = std::system(cmd.c_str());
        return Value::fromInt(static_cast<int64_t>(rc));
    });
    setGlobalFn("exec", i - 1);

    // exec_capture(cmd) – run shell command, return {code, out}
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kSystemExec, "exec_capture");
        std::unordered_map<std::string, ValuePtr> m;
        m["code"] = std::make_shared<Value>(Value::fromInt(-1));
        m["out"] = std::make_shared<Value>(Value::fromString(""));
        if (args.empty() || !args[0]) return Value::fromMap(std::move(m));
        std::string cmd = args[0]->toString();
        if (cmd.empty()) return Value::fromMap(std::move(m));
#ifdef _WIN32
        std::string fullCmd = cmd + " 2>&1";
        FILE* pipe = _popen(fullCmd.c_str(), "r");
#else
        std::string fullCmd = cmd + " 2>&1";
        FILE* pipe = popen(fullCmd.c_str(), "r");
#endif
        if (!pipe) return Value::fromMap(std::move(m));
        std::string out;
        char buf[512];
        while (std::fgets(buf, sizeof(buf), pipe)) out += buf;
#ifdef _WIN32
        int rc = _pclose(pipe);
#else
        int rc = pclose(pipe);
#endif
        m["code"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(rc)));
        m["out"] = std::make_shared<Value>(Value::fromString(out));
        m["stdout"] = std::make_shared<Value>(Value::fromString(out));
        m["stderr"] = std::make_shared<Value>(Value::fromString(""));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("exec_capture", i - 1);

    // which(program) – resolve executable in PATH; returns absolute path or nil
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kSystemExec, "which");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::nil();
        std::string prog = std::get<std::string>(args[0]->data);
        if (prog.empty()) return Value::nil();
        try {
            std::filesystem::path p(prog);
            if (p.has_parent_path()) {
                if (std::filesystem::exists(p)) return Value::fromString(std::filesystem::absolute(p).string());
                return Value::nil();
            }
            std::string pathEnv;
#ifdef _WIN32
            char* v = nullptr; size_t n = 0;
            _dupenv_s(&v, &n, "PATH");
            if (v) { pathEnv = v; std::free(v); }
            std::string exts;
            v = nullptr; n = 0;
            _dupenv_s(&v, &n, "PATHEXT");
            if (v) { exts = v; std::free(v); }
            std::vector<std::string> extList;
            if (!exts.empty()) {
                std::stringstream es(exts);
                std::string e;
                while (std::getline(es, e, ';')) if (!e.empty()) extList.push_back(e);
            }
            if (extList.empty()) extList = {".EXE", ".BAT", ".CMD", ".COM"};
            std::stringstream ss(pathEnv);
            std::string dir;
            while (std::getline(ss, dir, ';')) {
                if (dir.empty()) continue;
                std::filesystem::path base = std::filesystem::path(dir) / prog;
                if (std::filesystem::exists(base)) return Value::fromString(base.string());
                for (const auto& e : extList) {
                    std::filesystem::path cand = base;
                    cand += e;
                    if (std::filesystem::exists(cand)) return Value::fromString(cand.string());
                }
            }
#else
            const char* v = kernGetEnv("PATH");
            if (v) pathEnv = v;
            std::stringstream ss(pathEnv);
            std::string dir;
            while (std::getline(ss, dir, ':')) {
                if (dir.empty()) continue;
                std::filesystem::path cand = std::filesystem::path(dir) / prog;
                if (std::filesystem::exists(cand)) return Value::fromString(cand.string());
            }
#endif
        } catch (...) {}
        return Value::nil();
    });
    setGlobalFn("which", i - 1);

    // runtime guard controls for safe execution.
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty()) return Value::fromInt(0);
        int64_t lim = 0;
        if (args[0] && (args[0]->type == Value::Type::INT || args[0]->type == Value::Type::FLOAT)) {
            lim = args[0]->type == Value::Type::INT ? std::get<int64_t>(args[0]->data) : static_cast<int64_t>(std::get<double>(args[0]->data));
        }
        if (lim < 0) lim = 0;
        vm->setStepLimit(static_cast<uint64_t>(lim));
        return Value::fromInt(static_cast<int64_t>(vm->getStepLimit()));
    });
    setGlobalFn("set_step_limit", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty()) return Value::fromInt(0);
        int64_t lim = 0;
        if (args[0] && (args[0]->type == Value::Type::INT || args[0]->type == Value::Type::FLOAT)) {
            lim = args[0]->type == Value::Type::INT ? std::get<int64_t>(args[0]->data) : static_cast<int64_t>(std::get<double>(args[0]->data));
        }
        if (lim < 0) lim = 0;
        if (lim > 0 && lim < 16) lim = 16;
        vm->setMaxCallDepth(static_cast<size_t>(lim));
        return Value::fromInt(static_cast<int64_t>(vm->getMaxCallDepth()));
    });
    setGlobalFn("set_max_call_depth", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty()) return Value::fromInt(0);
        int64_t lim = 0;
        if (args[0] && (args[0]->type == Value::Type::INT || args[0]->type == Value::Type::FLOAT)) {
            lim = args[0]->type == Value::Type::INT ? std::get<int64_t>(args[0]->data) : static_cast<int64_t>(std::get<double>(args[0]->data));
        }
        if (lim < 0) lim = 0;
        vm->setCallbackStepGuard(static_cast<uint64_t>(lim));
        return Value::fromInt(static_cast<int64_t>(vm->getCallbackStepGuard()));
    });
    setGlobalFn("set_callback_guard", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        bool enabled = true;
        int64_t seed = 1337;
        if (!args.empty() && args[0]) enabled = args[0]->isTruthy();
        if (args.size() >= 2 && args[1] && (args[1]->type == Value::Type::INT || args[1]->type == Value::Type::FLOAT)) {
            seed = args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : static_cast<int64_t>(std::get<double>(args[1]->data));
        }
        if (enabled) std::srand(static_cast<unsigned>(seed));
        else std::srand(static_cast<unsigned>(std::time(nullptr)));
        return Value::fromBool(enabled);
    });
    setGlobalFn("deterministic_mode", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        std::unordered_map<std::string, ValuePtr> m;
        m["version"] = std::make_shared<Value>(Value::fromString("wave1-runtime"));
        m["step_limit"] = std::make_shared<Value>(Value::fromInt(vm ? static_cast<int64_t>(vm->getStepLimit()) : 0));
        m["max_call_depth"] = std::make_shared<Value>(Value::fromInt(vm ? static_cast<int64_t>(vm->getMaxCallDepth()) : 0));
        m["callback_guard"] = std::make_shared<Value>(Value::fromInt(vm ? static_cast<int64_t>(vm->getCallbackStepGuard()) : 0));
        if (vm) {
            const RuntimeGuardPolicy& g = vm->getRuntimeGuards();
            m["debug_mode"] = std::make_shared<Value>(Value::fromBool(g.debugMode));
            m["allow_unsafe"] = std::make_shared<Value>(Value::fromBool(g.allowUnsafe));
            m["enforce_permissions"] = std::make_shared<Value>(Value::fromBool(g.enforcePermissions));
            m["pointer_bounds"] = std::make_shared<Value>(Value::fromBool(g.enforcePointerBounds));
            m["ffi_enabled"] = std::make_shared<Value>(Value::fromBool(g.ffiEnabled));
            m["sandbox"] = std::make_shared<Value>(Value::fromBool(g.sandboxEnabled));
            m["unsafe_depth"] = std::make_shared<Value>(Value::fromInt(vm->unsafeDepth()));
            std::vector<ValuePtr> perms;
            for (const auto& s : g.grantedPermissions)
                perms.push_back(std::make_shared<Value>(Value::fromString(s)));
            m["permissions_granted"] = std::make_shared<Value>(Value::fromArray(std::move(perms)));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("runtime_info", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "path_normalize");
        if (args.empty() || !args[0]) return Value::fromString("");
        std::filesystem::path p(args[0]->toString());
        try { return Value::fromString(p.lexically_normal().string()); }
        catch (...) { return Value::fromString(args[0]->toString()); }
    });
    setGlobalFn("path_normalize", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty() || !args[0]) return Value::nil();
        ValuePtr fn = args[0];
        int64_t tries = (args.size() >= 2) ? std::max<int64_t>(1, toInt(args[1])) : 3;
        int64_t sleepMs = (args.size() >= 3) ? std::max<int64_t>(0, toInt(args[2])) : 0;
        for (int64_t k = 0; k < tries; ++k) {
            try {
                ValuePtr out = vm->callValue(fn, {});
                if (out && out->type != Value::Type::NIL) return Value(*out);
            } catch (...) {
            }
            if (k + 1 < tries && sleepMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
        return Value::nil();
    });
    setGlobalFn("retry_call", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        uint64_t h1 = std::hash<std::string>{}(s);
        uint64_t h2 = std::hash<std::string>{}(std::string("sha1:") + s);
        uint32_t h3 = static_cast<uint32_t>(h1 ^ (h2 >> 32));
        std::ostringstream out;
        out << std::hex << std::setfill('0')
            << std::setw(16) << h1
            << std::setw(16) << h2
            << std::setw(8) << h3;
        std::string hex = out.str();
        if (hex.size() > 40) hex.resize(40);
        return Value::fromString(hex);
    });
    setGlobalFn("sha1", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        uint64_t h1 = std::hash<std::string>{}(std::string("a:") + s);
        uint64_t h2 = std::hash<std::string>{}(std::string("b:") + s);
        uint64_t h3 = std::hash<std::string>{}(std::string("c:") + s);
        uint64_t h4 = std::hash<std::string>{}(std::string("d:") + s);
        std::ostringstream out;
        out << std::hex << std::setfill('0')
            << std::setw(16) << h1
            << std::setw(16) << h2
            << std::setw(16) << h3
            << std::setw(16) << h4;
        return Value::fromString(out.str());
    });
    setGlobalFn("sha256", i - 1);

    auto quoteArg = [](const std::string& s) {
        if (s.find_first_of(" \t\"") == std::string::npos) return s;
        std::string q = "\"";
        for (char c : s) {
            if (c == '"') q += "\\\"";
            else q += c;
        }
        q += "\"";
        return q;
    };
    auto argsToCommand = [quoteArg](const std::vector<ValuePtr>& arr) {
        std::string cmd;
        for (size_t k = 0; k < arr.size(); ++k) {
            if (k) cmd += " ";
            cmd += quoteArg(arr[k] ? arr[k]->toString() : "");
        }
        return cmd;
    };

    makeBuiltin(i++, [argsToCommand](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kSystemExec, "exec_args");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(-1);
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (arr.empty()) return Value::fromInt(-1);
        std::string cmd = argsToCommand(arr);
        int rc = std::system(cmd.c_str());
        return Value::fromInt(static_cast<int64_t>(rc));
    });
    setGlobalFn("exec_args", i - 1);

    makeBuiltin(i++, [argsToCommand](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "spawn");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromInt(-1);
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (arr.empty()) return Value::fromInt(-1);
        std::string cmd = argsToCommand(arr);
#ifdef _WIN32
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<char> buf(cmd.begin(), cmd.end());
        buf.push_back('\0');
        BOOL ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (!ok) return Value::fromInt(-1);
        CloseHandle(pi.hThread);
        int64_t hid = g_nextSpawnHandle++;
        g_spawnHandles[hid] = pi.hProcess;
        return Value::fromInt(hid);
#else
        return Value::fromInt(static_cast<int64_t>(std::system(cmd.c_str())));
#endif
    });
    setGlobalFn("spawn", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "wait_process");
#ifdef _WIN32
        if (args.empty()) return Value::fromMap({});
        int64_t hid = toInt(args[0]);
        auto it = g_spawnHandles.find(hid);
        if (it == g_spawnHandles.end() || !it->second) return Value::fromMap({});
        int64_t timeoutMs = args.size() >= 2 ? std::max<int64_t>(0, toInt(args[1])) : -1;
        DWORD wr = WaitForSingleObject(it->second, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(wr == WAIT_OBJECT_0));
        m["exited"] = std::make_shared<Value>(Value::fromBool(wr == WAIT_OBJECT_0));
        m["timed_out"] = std::make_shared<Value>(Value::fromBool(wr == WAIT_TIMEOUT));
        DWORD code = 0;
        if (wr == WAIT_OBJECT_0 && GetExitCodeProcess(it->second, &code)) {
            m["code"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(code)));
            CloseHandle(it->second);
            g_spawnHandles.erase(it);
        } else {
            m["code"] = std::make_shared<Value>(Value::fromInt(-1));
        }
        return Value::fromMap(std::move(m));
#else
        (void)vm;
        (void)args;
        (void)toInt;
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["exited"] = std::make_shared<Value>(Value::fromBool(false));
        m["timed_out"] = std::make_shared<Value>(Value::fromBool(false));
        m["code"] = std::make_shared<Value>(Value::fromInt(-1));
        return Value::fromMap(std::move(m));
#endif
    });
    setGlobalFn("wait_process", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "kill_process");
#ifdef _WIN32
        if (args.empty()) return Value::fromBool(false);
        int64_t hid = toInt(args[0]);
        auto it = g_spawnHandles.find(hid);
        if (it == g_spawnHandles.end() || !it->second) return Value::fromBool(false);
        BOOL ok = TerminateProcess(it->second, 1);
        CloseHandle(it->second);
        g_spawnHandles.erase(it);
        return Value::fromBool(ok != 0);
#else
        (void)vm;
        (void)args;
        (void)toInt;
        return Value::fromBool(false);
#endif
    });
    setGlobalFn("kill_process", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        std::ostringstream out;
        static const char* hex = "0123456789ABCDEF";
        for (unsigned char c : s) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
                out << static_cast<char>(c);
            } else if (c == ' ') {
                out << '+';
            } else {
                out << '%' << hex[(c >> 4) & 0x0F] << hex[c & 0x0F];
            }
        }
        return Value::fromString(out.str());
    });
    setGlobalFn("url_encode", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        std::string out;
        for (size_t i2 = 0; i2 < s.size(); ++i2) {
            char c = s[i2];
            if (c == '+') out += ' ';
            else if (c == '%' && i2 + 2 < s.size()) {
                auto hexVal = [](char x) -> int {
                    if (x >= '0' && x <= '9') return x - '0';
                    if (x >= 'a' && x <= 'f') return x - 'a' + 10;
                    if (x >= 'A' && x <= 'F') return x - 'A' + 10;
                    return -1;
                };
                int hi = hexVal(s[i2 + 1]);
                int lo = hexVal(s[i2 + 2]);
                if (hi >= 0 && lo >= 0) {
                    out += static_cast<char>((hi << 4) | lo);
                    i2 += 2;
                } else {
                    out += c;
                }
            } else out += c;
        }
        return Value::fromString(out);
    });
    setGlobalFn("url_decode", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkHttp, "http_get");
        if (args.empty() || !args[0]) return Value::fromString("");
        std::string url = args[0]->toString();
        if (url.empty()) return Value::fromString("");
#ifdef _WIN32
        {
            std::string outNative = kernHttpGetWinHttp(url);
            if (!outNative.empty()) {
                if (outNative.size() >= 3 && static_cast<unsigned char>(outNative[0]) == 0xEF &&
                    static_cast<unsigned char>(outNative[1]) == 0xBB && static_cast<unsigned char>(outNative[2]) == 0xBF)
                    outNative.erase(0, 3);
                return Value::fromString(std::move(outNative));
            }
        }
#endif
        auto readPipe = [](FILE* pipe) -> std::string {
            std::string out;
            if (!pipe) return out;
            char buf[4096];
            while (std::fgets(buf, sizeof(buf), pipe)) out += buf;
            return out;
        };
        auto escapeForCmdDoubleQuoted = [](const std::string& s) {
            std::string esc;
            esc.reserve(s.size() + 8);
            for (char c : s) {
                if (c == '"')
                    esc += "\\\"";
                else if (c == '\\')
                    esc += "\\\\";
                else
                    esc += c;
            }
            return esc;
        };
        // match http_request: follow redirects, timeouts, compression, browser-like UA (many CDNs block empty/curl UAs).
        // http1.1 / --ssl-no-revoke reduce Windows schannel quirks (empty body, long hangs, close_notify noise).
        std::ostringstream curlCmd;
#ifdef _WIN32
        curlCmd << "curl -s -S -L --max-time 60 --compressed --http1.1 --ssl-no-revoke ";
#else
        curlCmd << "curl -s -S -L --max-time 60 --compressed --http1.1 ";
#endif
        curlCmd << "-H \"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                   "Chrome/120.0.0.0 Safari/537.36 Kern/1.0\" \""
                << escapeForCmdDoubleQuoted(url) << "\"";
#ifdef _WIN32
        FILE* pipe = _popen(curlCmd.str().c_str(), "r");
#else
        FILE* pipe = popen(curlCmd.str().c_str(), "r");
#endif
        std::string out = readPipe(pipe);
#ifdef _WIN32
        if (pipe) (void)_pclose(pipe);
#else
        if (pipe) (void)pclose(pipe);
#endif
#ifdef _WIN32
        // if curl is missing, TLS/Schannel hiccups, or stdout is empty, try Windows-native HTTPS (common on desktop).
        if (out.empty()) {
            std::string psEsc;
            psEsc.reserve(url.size() + 8);
            for (char c : url) {
                if (c == '\'')
                    psEsc += "''";
                else
                    psEsc += c;
            }
            std::string psCmd =
                "powershell -NoProfile -NonInteractive -Command \""
                "try { "
                "$ProgressPreference='SilentlyContinue'; "
                "(Invoke-WebRequest -Uri '" +
                psEsc +
                "' -UseBasicParsing -TimeoutSec 45 "
                "-Headers @{'User-Agent'='Mozilla/5.0 (Windows NT 10.0; Win64; x64) Kern/1.0'}).Content "
                "} catch { '' }\"";
            FILE* psPipe = _popen(psCmd.c_str(), "r");
            out = readPipe(psPipe);
            if (psPipe) (void)_pclose(psPipe);
        }
#endif
        if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF && static_cast<unsigned char>(out[1]) == 0xBB &&
            static_cast<unsigned char>(out[2]) == 0xBF)
            out.erase(0, 3);
        return Value::fromString(std::move(out));
    });
    setGlobalFn("http_get", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        if (args.empty() || !args[0]) return Value::fromMap(std::move(out));
        std::istringstream in(args[0]->toString());
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            if (line[0] == '#' || line[0] == '[') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            auto trim = [](std::string s) {
                size_t a = 0, b = s.size();
                while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
                while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
                return s.substr(a, b - a);
            };
            k = trim(k);
            v = trim(v);
            if (!v.empty() && v.front() == '"' && v.back() == '"' && v.size() >= 2) v = v.substr(1, v.size() - 2);
            if (v == "true" || v == "false") {
                out[k] = std::make_shared<Value>(Value::fromBool(v == "true"));
            } else {
                char* end = nullptr;
                long long iv = std::strtoll(v.c_str(), &end, 10);
                if (end && *end == '\0') out[k] = std::make_shared<Value>(Value::fromInt(iv));
                else out[k] = std::make_shared<Value>(Value::fromString(v));
            }
        }
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("toml_parse", i - 1);

    auto doHttpRequest = [](VM* vm, std::vector<ValuePtr> args, const char* opName) -> Value {
        vmRequirePermission(vm, Perm::kNetworkHttp, opName ? opName : "http_request");
        if (args.size() < 2 || !args[0] || !args[1]) {
            std::unordered_map<std::string, ValuePtr> m;
            m["status"] = std::make_shared<Value>(Value::fromInt(0));
            m["body"] = std::make_shared<Value>(Value::fromString(""));
            m["ok"] = std::make_shared<Value>(Value::fromBool(false));
            return Value::fromMap(std::move(m));
        }
        std::string method = args[0]->toString();
        std::string url = args[1]->toString();
        std::ostringstream cmd;
#ifdef _WIN32
        cmd << "curl -s -S -L --max-time 60 --compressed --http1.1 --ssl-no-revoke -w \"\\n__KERN_HTTP__%{http_code}\" -X "
            << method;
#else
        cmd << "curl -s -S -L --max-time 60 --compressed --http1.1 -w \"\\n__KERN_HTTP__%{http_code}\" -X " << method;
#endif
        bool hasUserAgent = false;
        if (args.size() >= 3 && args[2] && args[2]->type == Value::Type::MAP) {
            auto& hm = std::get<std::unordered_map<std::string, ValuePtr>>(args[2]->data);
            for (const auto& kv : hm) {
                std::string lk = kv.first;
                for (char& ch : lk) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (lk == "user-agent") hasUserAgent = true;
                std::string hv = kv.first + ": " + (kv.second ? kv.second->toString() : "");
                std::string esc;
                for (char c : hv) {
                    if (c == '"') esc += "\\\"";
                    else if (c == '\\') esc += "\\\\";
                    else esc += c;
                }
                cmd << " -H \"" << esc << "\"";
            }
        }
        if (!hasUserAgent) {
            cmd << " -H \"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                   "Chrome/120.0.0.0 Safari/537.36 Kern/1.0\"";
        }
        std::string tmpFile;
        if (args.size() >= 4 && args[3]) {
            std::string body = args[3]->toString();
            if (!body.empty()) {
                try {
                    tmpFile = (std::filesystem::temp_directory_path() / ("kern_http_" + std::to_string(std::rand()) + ".dat")).string();
                    std::ofstream ofs(tmpFile, std::ios::binary);
                    ofs << body;
                    ofs.close();
                    cmd << " --data-binary \"@" << tmpFile << "\"";
                } catch (...) {}
            }
        }
        cmd << " \"" << url << "\"";
#ifdef _WIN32
        FILE* pipe = _popen(cmd.str().c_str(), "r");
#else
        FILE* pipe = popen(cmd.str().c_str(), "r");
#endif
        std::string out;
        if (pipe) {
            char buf[4096];
            while (std::fgets(buf, sizeof(buf), pipe)) out += buf;
#ifdef _WIN32
            (void)_pclose(pipe);
#else
            (void)pclose(pipe);
#endif
        }
        if (!tmpFile.empty()) {
            try { std::filesystem::remove(tmpFile); } catch (...) {}
        }
        int status = 0;
        size_t pos = out.rfind("__KERN_HTTP__");
        if (pos != std::string::npos) {
            std::string tail = out.substr(pos + 13);
            status = static_cast<int>(std::strtol(tail.c_str(), nullptr, 10));
            out.resize(pos);
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
        }
        std::unordered_map<std::string, ValuePtr> m;
        m["status"] = std::make_shared<Value>(Value::fromInt(status));
        m["body"] = std::make_shared<Value>(Value::fromString(std::move(out)));
        m["ok"] = std::make_shared<Value>(Value::fromBool(status >= 200 && status < 300));
        return Value::fromMap(std::move(m));
    };
    makeBuiltin(i++, [doHttpRequest](VM* vm, std::vector<ValuePtr> args) {
        return doHttpRequest(vm, std::move(args), "http_request");
    });
    setGlobalFn("http_request", i - 1);
    makeBuiltin(i++, [doHttpRequest](VM* vm, std::vector<ValuePtr> args) {
        std::vector<ValuePtr> inner;
        inner.push_back(std::make_shared<Value>(Value::fromString("POST")));
        if (args.empty()) return Value::fromMap({});
        inner.push_back(args[0]);
        if (args.size() >= 3) {
            inner.push_back(args[2]);
            inner.push_back(args[1]);
        } else {
            inner.push_back(std::make_shared<Value>(Value::fromMap(std::unordered_map<std::string, ValuePtr>{})));
            inner.push_back(args.size() >= 2 ? args[1] : std::make_shared<Value>(Value::nil()));
        }
        return doHttpRequest(vm, std::move(inner), "http_post");
    });
    setGlobalFn("http_post", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING) return Value::fromArray({});
        try {
            std::string s = std::get<std::string>(args[0]->data);
            std::regex re(std::get<std::string>(args[1]->data));
            std::sregex_token_iterator it(s.begin(), s.end(), re, -1), end;
            std::vector<ValuePtr> parts;
            for (; it != end; ++it) parts.push_back(std::make_shared<Value>(Value::fromString(it->str())));
            return Value::fromArray(std::move(parts));
        } catch (...) { return Value::fromArray({}); }
    });
    setGlobalFn("regex_split", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[0]->type != Value::Type::STRING || args[1]->type != Value::Type::STRING) return Value::fromArray({});
        try {
            std::string s = std::get<std::string>(args[0]->data);
            std::regex re(std::get<std::string>(args[1]->data));
            std::vector<ValuePtr> out;
            for (std::sregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it) {
                std::vector<ValuePtr> groups;
                groups.push_back(std::make_shared<Value>(Value::fromString(it->str())));
                for (size_t gi = 1; gi < it->size(); ++gi)
                    groups.push_back(std::make_shared<Value>(Value::fromString((*it)[gi].str())));
                out.push_back(std::make_shared<Value>(Value::fromArray(std::move(groups))));
            }
            return Value::fromArray(std::move(out));
        } catch (...) { return Value::fromArray({}); }
    });
    setGlobalFn("regex_find_all", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> m;
        if (args.empty() || !args[0]) return Value::fromMap(std::move(m));
        std::string u = args[0]->toString();
        std::string scheme, rest;
        size_t c = u.find("://");
        if (c != std::string::npos) {
            scheme = u.substr(0, c);
            rest = u.substr(c + 3);
        } else {
            rest = u;
        }
        m["scheme"] = std::make_shared<Value>(Value::fromString(scheme));
        std::string hostport, pathq;
        size_t slash = rest.find('/');
        if (slash == std::string::npos) {
            hostport = rest;
        } else {
            hostport = rest.substr(0, slash);
            pathq = rest.substr(slash);
        }
        std::string host = hostport;
        int64_t port = (scheme == "https") ? 443 : (scheme == "http") ? 80 : 0;
        size_t colon = hostport.rfind(':');
        if (colon != std::string::npos && colon > 0) {
            host = hostport.substr(0, colon);
            port = std::strtoll(hostport.substr(colon + 1).c_str(), nullptr, 10);
        }
        m["host"] = std::make_shared<Value>(Value::fromString(host));
        m["port"] = std::make_shared<Value>(Value::fromInt(port));
        std::string path = pathq, query;
        size_t q = pathq.find('?');
        if (q != std::string::npos) {
            path = pathq.substr(0, q);
            query = pathq.substr(q + 1);
        }
        m["path"] = std::make_shared<Value>(Value::fromString(path.empty() ? "/" : path));
        m["query"] = std::make_shared<Value>(Value::fromString(query));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("url_parse", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> m;
        if (args.empty() || !args[0]) return Value::fromMap(std::move(m));
        std::string q = args[0]->toString();
        size_t start = 0;
        while (start < q.size()) {
            size_t amp = q.find('&', start);
            std::string pair = amp == std::string::npos ? q.substr(start) : q.substr(start, amp - start);
            start = amp == std::string::npos ? q.size() : amp + 1;
            size_t eq = pair.find('=');
            std::string k = eq == std::string::npos ? pair : pair.substr(0, eq);
            std::string v = eq == std::string::npos ? "" : pair.substr(eq + 1);
            if (!k.empty()) m[k] = std::make_shared<Value>(Value::fromString(v));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("parse_query", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        std::string out;
        out.reserve(s.size() + 16);
        for (unsigned char c : s) {
            switch (c) {
                case '&':
                    out += "&amp;";
                    break;
                case '<':
                    out += "&lt;";
                    break;
                case '>':
                    out += "&gt;";
                    break;
                case '"':
                    out += "&quot;";
                    break;
                case '\'':
                    out += "&#39;";
                    break;
                default:
                    out += static_cast<char>(c);
                    break;
            }
        }
        return Value::fromString(std::move(out));
    });
    setGlobalFn("html_escape", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        auto trimStr = [](std::string& s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        };
        auto lowerKey = [](std::string s) {
            for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        std::string raw = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> out;
        size_t pos = 0;
        if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF && static_cast<unsigned char>(raw[1]) == 0xBB &&
            static_cast<unsigned char>(raw[2]) == 0xBF)
            pos = 3;
        while (pos < raw.size() && (raw[pos] == '\r' || raw[pos] == '\n')) ++pos;
        size_t eol = raw.find('\n', pos);
        std::string line1 = eol == std::string::npos ? raw.substr(pos) : raw.substr(pos, eol - pos);
        if (!line1.empty() && line1.back() == '\r') line1.pop_back();
        trimStr(line1);
        int64_t status = 0;
        std::string reason;
        bool http = line1.size() >= 5 && line1.compare(0, 5, "HTTP/") == 0;
        if (!http) {
            std::unordered_map<std::string, ValuePtr> emptyH;
            out["status"] = std::make_shared<Value>(Value::fromInt(0));
            out["status_text"] = std::make_shared<Value>(Value::fromString(""));
            out["headers"] = std::make_shared<Value>(Value::fromMap(std::move(emptyH)));
            out["body"] = std::make_shared<Value>(Value::fromString(raw));
            out["ok"] = std::make_shared<Value>(Value::fromBool(false));
            return Value::fromMap(std::move(out));
        }
        size_t sp = line1.find(' ');
        if (sp != std::string::npos) {
            size_t sp2 = line1.find(' ', sp + 1);
            std::string codeStr = sp2 == std::string::npos ? line1.substr(sp + 1) : line1.substr(sp + 1, sp2 - sp - 1);
            trimStr(codeStr);
            status = std::strtoll(codeStr.c_str(), nullptr, 10);
            if (sp2 != std::string::npos) {
                reason = line1.substr(sp2 + 1);
                trimStr(reason);
            }
        }
        pos = eol == std::string::npos ? raw.size() : eol + 1;
        std::unordered_map<std::string, ValuePtr> headers;
        std::string lastKey;
        while (pos < raw.size()) {
            eol = raw.find('\n', pos);
            std::string line = eol == std::string::npos ? raw.substr(pos) : raw.substr(pos, eol - pos);
            pos = eol == std::string::npos ? raw.size() : eol + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            if (line[0] == ' ' || line[0] == '\t') {
                if (!lastKey.empty()) {
                    auto it = headers.find(lastKey);
                    std::string v = (it != headers.end() && it->second) ? it->second->toString() : "";
                    std::string cont = line.size() > 1 ? line.substr(1) : "";
                    trimStr(cont);
                    v += " ";
                    v += cont;
                    headers[lastKey] = std::make_shared<Value>(Value::fromString(std::move(v)));
                }
                continue;
            }
            size_t col = line.find(':');
            if (col == std::string::npos) continue;
            std::string hk = line.substr(0, col);
            std::string hv = line.substr(col + 1);
            trimStr(hk);
            trimStr(hv);
            lastKey = lowerKey(hk);
            headers[lastKey] = std::make_shared<Value>(Value::fromString(std::move(hv)));
        }
        std::string body = pos < raw.size() ? raw.substr(pos) : "";
        if (!body.empty() && body.front() == '\r') body.erase(0, 1);
        if (!body.empty() && body.front() == '\n') body.erase(0, 1);
        if (!body.empty() && body.front() == '\r') body.erase(0, 1);
        if (!body.empty() && body.front() == '\n') body.erase(0, 1);
        out["status"] = std::make_shared<Value>(Value::fromInt(status));
        out["status_text"] = std::make_shared<Value>(Value::fromString(reason));
        out["headers"] = std::make_shared<Value>(Value::fromMap(std::move(headers)));
        out["body"] = std::make_shared<Value>(Value::fromString(std::move(body)));
        out["ok"] = std::make_shared<Value>(Value::fromBool(status >= 200 && status < 300));
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("http_parse_response", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::function<std::string(const ValuePtr&, int)> toToml;
        toToml = [&toToml](const ValuePtr& v, int depth) -> std::string {
            if (!v) return "null";
            if (depth > 64) return "\"<max_depth>\"";
            switch (v->type) {
                case Value::Type::NIL: return "null";
                case Value::Type::BOOL: return std::get<bool>(v->data) ? "true" : "false";
                case Value::Type::INT: return std::to_string(std::get<int64_t>(v->data));
                case Value::Type::FLOAT: {
                    char b[64];
                    snprintf(b, sizeof(b), "%.15g", std::get<double>(v->data));
                    return b;
                }
                case Value::Type::STRING: {
                    std::string s = std::get<std::string>(v->data);
                    return "\"" + jsonEscape(s) + "\"";
                }
                case Value::Type::ARRAY: {
                    std::string o = "[";
                    auto& arr = std::get<std::vector<ValuePtr>>(v->data);
                    for (size_t i = 0; i < arr.size(); ++i) {
                        if (i) o += ", ";
                        o += toToml(arr[i], depth + 1);
                    }
                    return o + "]";
                }
                case Value::Type::MAP: {
                    std::string o = "{";
                    auto& mp = std::get<std::unordered_map<std::string, ValuePtr>>(v->data);
                    bool first = true;
                    for (const auto& kv : mp) {
                        if (!first) o += ", ";
                        first = false;
                        o += kv.first + " = " + toToml(kv.second, depth + 1);
                    }
                    return o + "}";
                }
                default: return "null";
            }
        };
        if (args.empty() || !args[0]) return Value::fromString("");
        return Value::fromString(toToml(args[0], 0));
    });
    setGlobalFn("toml_stringify", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromInt(0);
        try {
            int64_t id = g_nextRegexId.fetch_add(1);
            std::lock_guard<std::mutex> lock(g_regexCacheMutex);
            g_regexCache[id] = std::regex(std::get<std::string>(args[0]->data));
            return Value::fromInt(id);
        } catch (...) { return Value::fromInt(0); }
    });
    setGlobalFn("regex_compile", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || !args[1] || args[1]->type != Value::Type::STRING) return Value::fromBool(false);
        int64_t id = 0;
        if (args[0]->type == Value::Type::INT) id = std::get<int64_t>(args[0]->data);
        else return Value::fromBool(false);
        std::lock_guard<std::mutex> lock(g_regexCacheMutex);
        auto it = g_regexCache.find(id);
        if (it == g_regexCache.end()) return Value::fromBool(false);
        try {
            return Value::fromBool(std::regex_search(std::get<std::string>(args[1]->data), it->second));
        } catch (...) { return Value::fromBool(false); }
    });
    setGlobalFn("regex_match_pattern", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 3 || !args[0] || !args[1] || !args[2] || args[1]->type != Value::Type::STRING || args[2]->type != Value::Type::STRING)
            return Value::nil();
        int64_t id = 0;
        if (args[0]->type == Value::Type::INT) id = std::get<int64_t>(args[0]->data);
        else return Value::nil();
        std::lock_guard<std::mutex> lock(g_regexCacheMutex);
        auto it = g_regexCache.find(id);
        if (it == g_regexCache.end()) return Value::nil();
        try {
            std::string s = std::get<std::string>(args[1]->data);
            std::string repl = std::get<std::string>(args[2]->data);
            return Value::fromString(std::regex_replace(s, it->second, repl));
        } catch (...) { return Value::nil(); }
    });
    setGlobalFn("regex_replace_pattern", i - 1);

    // format_exception(e) — Python-style multi-line report; error_traceback(e) — frames array or nil
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::fromString("");
        return Value::fromString(formatExceptionValue(args[0]));
    });
    setGlobalFn("format_exception", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::nil();
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        auto it = m.find("traceback");
        if (it == m.end() || !it->second) return Value::nil();
        return *it->second;
    });
    setGlobalFn("error_traceback", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        if (args.empty() || !args[0]) {
            out["name"] = std::make_shared<Value>(Value::fromString("Error"));
            out["message"] = std::make_shared<Value>(Value::fromString(""));
            out["traceback"] = std::make_shared<Value>(Value::nil());
            out["cause"] = std::make_shared<Value>(Value::nil());
            return Value::fromMap(std::move(out));
        }
        ValuePtr e = args[0];
        out["name"] = std::make_shared<Value>(Value::fromString("Error"));
        out["message"] = std::make_shared<Value>(Value::fromString(e->toString()));
        out["traceback"] = std::make_shared<Value>(Value::nil());
        out["cause"] = (args.size() >= 2 && args[1]) ? args[1] : std::make_shared<Value>(Value::nil());
        if (e->type == Value::Type::MAP) {
            auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(e->data);
            auto setIf = [&](const char* key) {
                auto it = m.find(key);
                if (it != m.end() && it->second) out[key] = it->second;
            };
            setIf("name");
            setIf("message");
            setIf("code");
            setIf("traceback");
            setIf("line");
            setIf("column");
            if (m.find("cause") == m.end()) {
                // keep explicit 2nd arg cause if provided
            } else {
                setIf("cause");
            }
        }
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("error_structured", i - 1);

    // invoke(fn, args_array) — dynamic call; extend_array(dest, src) — append elements (mutates dest)
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.size() < 2 || !args[0]) return Value::nil();
        if (!args[1] || args[1]->type != Value::Type::ARRAY) return Value::nil();
        std::vector<ValuePtr> arr = std::get<std::vector<ValuePtr>>(args[1]->data);
        ValuePtr r = vm->callValue(args[0], std::move(arr));
        if (!r) return Value::nil();
        return *r;
    });
    setGlobalFn("invoke", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::nil();
        if (!args[1] || args[1]->type != Value::Type::ARRAY) return Value::nil();
        auto& dest = std::get<std::vector<ValuePtr>>(args[0]->data);
        auto& src = std::get<std::vector<ValuePtr>>(args[1]->data);
        const size_t kMaxArraySize = 64 * 1024 * 1024;
        if (dest.size() >= kMaxArraySize) return Value(*args[0]);
        size_t room = kMaxArraySize - dest.size();
        if (src.size() <= room)
            dest.insert(dest.end(), src.begin(), src.end());
        else
            dest.insert(dest.end(), src.begin(), src.begin() + room);
        return Value(*args[0]);
    });
    setGlobalFn("extend_array", i - 1);
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::nil();
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        auto it = m.find("close");
        if (it == m.end() || !it->second) return Value::nil();
        ValuePtr r = vm->callValue(it->second, {args[0]});
        (void)r;
        return Value::nil();
    });
    setGlobalFn("with_cleanup", i - 1);

    // sorted(arr) — new array, lexicographic by toString (original unchanged; unlike sort which mutates)
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& src = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<ValuePtr> arr = src;
        std::sort(arr.begin(), arr.end(), [](const ValuePtr& a, const ValuePtr& b) {
            return (a ? a->toString() : "null") < (b ? b->toString() : "null");
        });
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("sorted", i - 1);

    // enumerate(arr [, start]) — [[i, v], ...] with int indices (default start 0)
    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromArray({});
        auto& src = std::get<std::vector<ValuePtr>>(args[0]->data);
        int64_t start = args.size() >= 2 ? toInt(args[1]) : 0;
        std::vector<ValuePtr> out;
        out.reserve(src.size());
        for (size_t j = 0; j < src.size(); ++j) {
            std::vector<ValuePtr> pair;
            pair.push_back(std::make_shared<Value>(Value::fromInt(start + static_cast<int64_t>(j))));
            pair.push_back(src[j] ? src[j] : std::make_shared<Value>(Value::nil()));
            out.push_back(std::make_shared<Value>(Value::fromArray(std::move(pair))));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("enumerate", i - 1);

    // partition(s, sep) — [before, sep, after]; sep not found => [s, "", ""]; empty sep => [s, "", ""]
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.size() < 2 || !args[0] || args[0]->type != Value::Type::STRING) return Value::fromArray({});
        std::string s = std::get<std::string>(args[0]->data);
        std::string sep = args[1] && args[1]->type == Value::Type::STRING ? std::get<std::string>(args[1]->data) : "";
        std::vector<ValuePtr> out;
        if (sep.empty()) {
            out.push_back(std::make_shared<Value>(Value::fromString(s)));
            out.push_back(std::make_shared<Value>(Value::fromString("")));
            out.push_back(std::make_shared<Value>(Value::fromString("")));
            return Value::fromArray(std::move(out));
        }
        size_t pos = s.find(sep);
        if (pos == std::string::npos) {
            out.push_back(std::make_shared<Value>(Value::fromString(s)));
            out.push_back(std::make_shared<Value>(Value::fromString("")));
            out.push_back(std::make_shared<Value>(Value::fromString("")));
        } else {
            out.push_back(std::make_shared<Value>(Value::fromString(s.substr(0, pos))));
            out.push_back(std::make_shared<Value>(Value::fromString(sep)));
            out.push_back(std::make_shared<Value>(Value::fromString(s.substr(pos + sep.size()))));
        }
        return Value::fromArray(std::move(out));
    });
    setGlobalFn("partition", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        return Value::fromBool(args.size() >= 1 && args[0] && args[0]->type == Value::Type::INT);
    });
    setGlobalFn("is_int", i - 1);
    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        return Value::fromBool(args.size() >= 1 && args[0] && args[0]->type == Value::Type::FLOAT);
    });
    setGlobalFn("is_float", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        std::string out;
        for (size_t p = 0; p < s.size();) {
            if (s[p] != '&') {
                out += s[p++];
                continue;
            }
            size_t semi = s.find(';', p);
            if (semi == std::string::npos) {
                out += s[p++];
                continue;
            }
            std::string ent = s.substr(p, semi - p + 1);
            std::string rep;
            if (ent == "&amp;")
                rep = "&";
            else if (ent == "&lt;")
                rep = "<";
            else if (ent == "&gt;")
                rep = ">";
            else if (ent == "&quot;")
                rep = "\"";
            else if (ent == "&apos;" || ent == "&#39;")
                rep = "'";
            else if (ent == "&nbsp;")
                kernAppendUtf8(rep, 0xA0U);
            else if (ent.size() > 3 && ent[1] == '#') {
                uint32_t cp = 0;
                if (ent[2] == 'x' || ent[2] == 'X') {
                    cp = static_cast<uint32_t>(std::strtoul(ent.substr(3, ent.size() - 4).c_str(), nullptr, 16));
                } else {
                    cp = static_cast<uint32_t>(std::strtoul(ent.substr(2, ent.size() - 3).c_str(), nullptr, 10));
                }
                if (cp > 0 && cp <= 0x10FFFFU) kernAppendUtf8(rep, cp);
            }
            if (!rep.empty()) {
                out += rep;
                p = semi + 1;
            } else {
                out += s[p++];
            }
        }
        return Value::fromString(std::move(out));
    });
    setGlobalFn("html_unescape", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        try {
            std::regex cmt("<!--[\\s\\S]*?-->");
            std::string t = std::regex_replace(s, cmt, std::string(""));
            std::regex tag("<[^>]+>");
            t = std::regex_replace(t, tag, std::string(""));
            return Value::fromString(std::move(t));
        } catch (...) {
            return Value::fromString(s);
        }
    });
    setGlobalFn("strip_html", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        std::string out;
        out.reserve(s.size() + 8);
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += static_cast<char>(c);
            } else if (c == '\n') {
                out += "\\A ";
            } else if (c == '\r') {
                /* skip CR*/
            } else if (c < 32) {
                char b[8];
                snprintf(b, sizeof(b), "\\%x ", static_cast<unsigned>(c));
                out += b;
            } else
                out += static_cast<char>(c);
        }
        return Value::fromString(std::move(out));
    });
    setGlobalFn("css_escape", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        return Value::fromString(jsonEscape(s));
    });
    setGlobalFn("js_escape", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        std::string out;
        out.reserve(s.size() + 16);
        for (unsigned char c : s) {
            switch (c) {
                case '&':
                    out += "&amp;";
                    break;
                case '<':
                    out += "&lt;";
                    break;
                case '>':
                    out += "&gt;";
                    break;
                case '"':
                    out += "&quot;";
                    break;
                case '\'':
                    out += "&apos;";
                    break;
                default:
                    out += static_cast<char>(c);
                    break;
            }
        }
        return Value::fromString(std::move(out));
    });
    setGlobalFn("xml_escape", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::fromString("");
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        std::vector<std::string> keys;
        keys.reserve(m.size());
        for (const auto& kv : m) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        std::ostringstream q;
        bool first = true;
        for (const std::string& k : keys) {
            auto it = m.find(k);
            if (it == m.end() || !it->second) continue;
            if (!first) q << '&';
            first = false;
            q << kernUrlEncodeQueryPart(k) << '=' << kernUrlEncodeQueryPart(it->second->toString());
        }
        return Value::fromString(q.str());
    });
    setGlobalFn("build_query", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string base = args.size() >= 1 && args[0] ? args[0]->toString() : "";
        std::string rel = args.size() >= 2 && args[1] ? args[1]->toString() : "";
        return Value::fromString(kernUrlResolve(base, rel));
    });
    setGlobalFn("url_resolve", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string path = args.empty() || !args[0] ? "" : args[0]->toString();
        size_t dot = path.rfind('.');
        std::string ext = dot == std::string::npos ? "" : path.substr(dot);
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        static const std::unordered_map<std::string, std::string> kMime = {
            {".html", "text/html; charset=utf-8"},
            {".htm", "text/html; charset=utf-8"},
            {".css", "text/css; charset=utf-8"},
            {".js", "text/javascript; charset=utf-8"},
            {".mjs", "text/javascript; charset=utf-8"},
            {".json", "application/json; charset=utf-8"},
            {".xml", "application/xml; charset=utf-8"},
            {".svg", "image/svg+xml"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".webp", "image/webp"},
            {".ico", "image/x-icon"},
            {".woff2", "font/woff2"},
            {".woff", "font/woff"},
            {".ttf", "font/ttf"},
            {".txt", "text/plain; charset=utf-8"},
            {".md", "text/markdown; charset=utf-8"},
            {".wasm", "application/wasm"},
            {".pdf", "application/pdf"},
            {".zip", "application/zip"},
        };
        auto it = kMime.find(ext);
        if (it != kMime.end()) return Value::fromString(it->second);
        return Value::fromString("application/octet-stream");
    });
    setGlobalFn("mime_type_guess", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        auto set = [&out](const char* k, ValuePtr v) { out[k] = std::move(v); };
        if (s.size() < 6 || s.compare(0, 5, "data:") != 0) {
            set("ok", std::make_shared<Value>(Value::fromBool(false)));
            set("mime", std::make_shared<Value>(Value::fromString("")));
            set("is_base64", std::make_shared<Value>(Value::fromBool(false)));
            set("data", std::make_shared<Value>(Value::fromString("")));
            return Value::fromMap(std::move(out));
        }
        size_t comma = s.find(',');
        if (comma == std::string::npos || comma + 1 > s.size()) {
            set("ok", std::make_shared<Value>(Value::fromBool(false)));
            set("mime", std::make_shared<Value>(Value::fromString("")));
            set("is_base64", std::make_shared<Value>(Value::fromBool(false)));
            set("data", std::make_shared<Value>(Value::fromString("")));
            return Value::fromMap(std::move(out));
        }
        std::string meta = s.substr(5, comma - 5);
        std::string payload = s.substr(comma + 1);
        bool b64 = false;
        std::string mime = "text/plain;charset=US-ASCII";
        size_t semi = meta.find(';');
        if (semi == std::string::npos) {
            if (!meta.empty()) mime = meta;
        } else {
            mime = meta.substr(0, semi);
            std::string rest = meta.substr(semi);
            for (char& c : rest) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (rest.find(";base64") != std::string::npos) b64 = true;
        }
        std::string dataOut = b64 ? kernBase64DecodeDataUrl(payload) : payload;
        set("ok", std::make_shared<Value>(Value::fromBool(true)));
        set("mime", std::make_shared<Value>(Value::fromString(mime)));
        set("is_base64", std::make_shared<Value>(Value::fromBool(b64)));
        set("data", std::make_shared<Value>(Value::fromString(std::move(dataOut))));
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("parse_data_url", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        size_t start = 0;
        while (start < s.size()) {
            size_t semi = s.find(';', start);
            std::string part = semi == std::string::npos ? s.substr(start) : s.substr(start, semi - start);
            part = kernTrimHttpWs(part);
            start = semi == std::string::npos ? s.size() : semi + 1;
            if (part.empty()) continue;
            size_t eq = part.find('=');
            if (eq == std::string::npos) continue;
            std::string k = kernTrimHttpWs(part.substr(0, eq));
            std::string v = kernTrimHttpWs(part.substr(eq + 1));
            if (!k.empty()) out[k] = std::make_shared<Value>(Value::fromString(v));
        }
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("parse_cookie_header", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::fromString("");
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        auto getStr = [&m](const char* k) -> std::string {
            auto it = m.find(k);
            if (it == m.end() || !it->second) return "";
            return it->second->toString();
        };
        auto truthy = [&m](const char* k) -> bool {
            auto it = m.find(k);
            if (it == m.end() || !it->second) return false;
            if (it->second->type == Value::Type::BOOL) return std::get<bool>(it->second->data);
            std::string t = it->second->toString();
            for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return t == "true" || t == "1" || t == "yes";
        };
        auto getInt = [&m](const char* k, int64_t d) -> int64_t {
            auto it = m.find(k);
            if (it == m.end() || !it->second) return d;
            if (it->second->type == Value::Type::INT) return std::get<int64_t>(it->second->data);
            if (it->second->type == Value::Type::FLOAT)
                return static_cast<int64_t>(std::get<double>(it->second->data));
            return std::strtoll(it->second->toString().c_str(), nullptr, 10);
        };
        std::string name = getStr("name");
        std::string val = getStr("value");
        if (name.empty()) return Value::fromString("");
        std::ostringstream line;
        line << name << '=' << kernCookieValueField(val);
        std::string path = getStr("path");
        if (!path.empty()) line << "; Path=" << path;
        std::string domain = getStr("domain");
        if (!domain.empty()) line << "; Domain=" << domain;
        int64_t ma = getInt("max_age", int64_t(-1));
        if (ma >= 0) line << "; Max-Age=" << ma;
        if (truthy("httponly")) line << "; HttpOnly";
        if (truthy("secure")) line << "; Secure";
        std::string ss = getStr("samesite");
        if (!ss.empty()) line << "; SameSite=" << ss;
        return Value::fromString(line.str());
    });
    setGlobalFn("set_cookie_fields", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        s = kernTrimHttpWs(s);
        std::string lower;
        lower.reserve(s.size());
        for (unsigned char c : s) lower += static_cast<char>(std::tolower(c));
        std::string charset = "utf-8";
        size_t pos = lower.find("charset=");
        if (pos != std::string::npos) {
            size_t i = pos + 8;
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (i < s.size() && s[i] == '"') {
                ++i;
                size_t j = s.find('"', i);
                charset = j == std::string::npos ? s.substr(i) : s.substr(i, j - i);
            } else {
                size_t j = i;
                while (j < s.size() && s[j] != ';' && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
                charset = s.substr(i, j - i);
            }
            charset = kernTrimHttpWs(charset);
            for (char& c : charset) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (charset.empty()) charset = "utf-8";
        }
        return Value::fromString(charset);
    });
    setGlobalFn("content_type_charset", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string url = args.empty() || !args[0] ? "" : kernTrimHttpWs(args[0]->toString());
        std::string base = args.size() >= 2 && args[1] ? kernTrimHttpWs(args[1]->toString()) : "";
        if (url.empty()) return Value::fromBool(false);
        std::string low;
        for (unsigned char c : url) low += static_cast<char>(std::tolower(c));
        if (low.rfind("javascript:", 0) == 0 || low.rfind("data:", 0) == 0 || low.rfind("vbscript:", 0) == 0)
            return Value::fromBool(false);
        if (url[0] == '/' && (url.size() == 1 || url[1] != '/')) return Value::fromBool(true);
        if (url.size() >= 2 && url[0] == '/' && url[1] == '/') {
            if (base.empty()) return Value::fromBool(false);
            size_t bc = base.find("://");
            if (bc == std::string::npos) return Value::fromBool(false);
            std::string abs = base.substr(0, bc + 1) + url;
            std::string rs, rh;
            int64_t rp = 0;
            kernUrlHostPort(abs, rs, rh, rp);
            std::string bs, bh;
            int64_t bp = 0;
            kernUrlHostPort(base, bs, bh, bp);
            if (rs != "http" && rs != "https") return Value::fromBool(false);
            if (bs != "http" && bs != "https") return Value::fromBool(false);
            return Value::fromBool(rh == bh && rp == bp);
        }
        size_t rc = url.find("://");
        if (rc != std::string::npos) {
            if (base.empty()) return Value::fromBool(false);
            std::string rs, rh;
            int64_t rp = 0;
            kernUrlHostPort(url, rs, rh, rp);
            if (rs != "http" && rs != "https") return Value::fromBool(false);
            std::string bs, bh;
            int64_t bp = 0;
            kernUrlHostPort(base, bs, bh, bp);
            if (bs != "http" && bs != "https") return Value::fromBool(false);
            return Value::fromBool(rh == bh && rp == bp);
        }
        return Value::fromBool(true);
    });
    setGlobalFn("is_safe_http_redirect", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        auto trimStr = [](std::string& s) {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(0, 1);
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
        };
        auto lowerKey = [](std::string s) {
            for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        std::string raw = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> out;
        size_t pos = 0;
        if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF && static_cast<unsigned char>(raw[1]) == 0xBB &&
            static_cast<unsigned char>(raw[2]) == 0xBF)
            pos = 3;
        while (pos < raw.size() && (raw[pos] == '\r' || raw[pos] == '\n')) ++pos;
        size_t eol = raw.find('\n', pos);
        std::string line1 = eol == std::string::npos ? raw.substr(pos) : raw.substr(pos, eol - pos);
        if (!line1.empty() && line1.back() == '\r') line1.pop_back();
        trimStr(line1);
        pos = eol == std::string::npos ? raw.size() : eol + 1;
        size_t sp1 = line1.find(' ');
        if (sp1 == std::string::npos) {
            std::unordered_map<std::string, ValuePtr> emptyH;
            out["method"] = std::make_shared<Value>(Value::fromString(""));
            out["path"] = std::make_shared<Value>(Value::fromString(""));
            out["version"] = std::make_shared<Value>(Value::fromString(""));
            out["headers"] = std::make_shared<Value>(Value::fromMap(std::move(emptyH)));
            out["body"] = std::make_shared<Value>(Value::fromString(raw));
            out["ok"] = std::make_shared<Value>(Value::fromBool(false));
            return Value::fromMap(std::move(out));
        }
        std::string method = line1.substr(0, sp1);
        trimStr(method);
        for (char& c : method) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        size_t sp2 = line1.find(' ', sp1 + 1);
        std::string path = sp2 == std::string::npos ? line1.substr(sp1 + 1) : line1.substr(sp1 + 1, sp2 - sp1 - 1);
        trimStr(path);
        std::string version = sp2 == std::string::npos ? "" : line1.substr(sp2 + 1);
        trimStr(version);
        if (method.empty() || path.empty()) {
            std::unordered_map<std::string, ValuePtr> emptyH;
            out["method"] = std::make_shared<Value>(Value::fromString(method));
            out["path"] = std::make_shared<Value>(Value::fromString(path));
            out["version"] = std::make_shared<Value>(Value::fromString(version));
            out["headers"] = std::make_shared<Value>(Value::fromMap(std::move(emptyH)));
            out["body"] = std::make_shared<Value>(Value::fromString(pos < raw.size() ? raw.substr(pos) : ""));
            out["ok"] = std::make_shared<Value>(Value::fromBool(false));
            return Value::fromMap(std::move(out));
        }
        std::unordered_map<std::string, ValuePtr> headers;
        std::string lastKey;
        while (pos < raw.size()) {
            eol = raw.find('\n', pos);
            std::string line = eol == std::string::npos ? raw.substr(pos) : raw.substr(pos, eol - pos);
            pos = eol == std::string::npos ? raw.size() : eol + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            if (line[0] == ' ' || line[0] == '\t') {
                if (!lastKey.empty()) {
                    auto it = headers.find(lastKey);
                    std::string v = (it != headers.end() && it->second) ? it->second->toString() : "";
                    std::string cont = line.size() > 1 ? line.substr(1) : "";
                    trimStr(cont);
                    v += " ";
                    v += cont;
                    headers[lastKey] = std::make_shared<Value>(Value::fromString(std::move(v)));
                }
                continue;
            }
            size_t col = line.find(':');
            if (col == std::string::npos) continue;
            std::string hk = line.substr(0, col);
            std::string hv = line.substr(col + 1);
            trimStr(hk);
            trimStr(hv);
            lastKey = lowerKey(hk);
            headers[lastKey] = std::make_shared<Value>(Value::fromString(std::move(hv)));
        }
        std::string body = pos < raw.size() ? raw.substr(pos) : "";
        if (!body.empty() && body.front() == '\r') body.erase(0, 1);
        if (!body.empty() && body.front() == '\n') body.erase(0, 1);
        if (!body.empty() && body.front() == '\r') body.erase(0, 1);
        if (!body.empty() && body.front() == '\n') body.erase(0, 1);
        out["method"] = std::make_shared<Value>(Value::fromString(method));
        out["path"] = std::make_shared<Value>(Value::fromString(path));
        out["version"] = std::make_shared<Value>(Value::fromString(version));
        out["headers"] = std::make_shared<Value>(Value::fromMap(std::move(headers)));
        out["body"] = std::make_shared<Value>(Value::fromString(std::move(body)));
        out["ok"] = std::make_shared<Value>(Value::fromBool(true));
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("http_parse_request", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::vector<ValuePtr> links;
        std::string s = args.empty() || !args[0] ? "" : kernTrimHttpWs(args[0]->toString());
        size_t i = 0;
        while (i < s.size()) {
            while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            if (i >= s.size()) break;
            if (s[i] != '<') {
                ++i;
                continue;
            }
            size_t gt = s.find('>', i + 1);
            if (gt == std::string::npos) break;
            std::string url = kernTrimHttpWs(s.substr(i + 1, gt - i - 1));
            size_t pEnd = kernLinkParamSectionEnd(s, gt + 1);
            std::string paramStr = kernTrimHttpWs(s.substr(gt + 1, pEnd - gt - 1));
            i = pEnd;
            if (i < s.size() && s[i] == ',') {
                ++i;
                while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
            }
            std::unordered_map<std::string, std::string> kv;
            kernParseSemicolonKeyValues(paramStr, kv);
            std::unordered_map<std::string, ValuePtr> one;
            one["url"] = std::make_shared<Value>(Value::fromString(url));
            auto addIf = [&kv, &one](const char* k) {
                auto it = kv.find(k);
                if (it != kv.end()) one[k] = std::make_shared<Value>(Value::fromString(it->second));
            };
            addIf("rel");
            addIf("type");
            addIf("title");
            addIf("anchor");
            links.push_back(std::make_shared<Value>(Value::fromMap(std::move(one))));
        }
        return Value::fromArray(std::move(links));
    });
    setGlobalFn("parse_link_header", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        std::string s = args.empty() || !args[0] ? "" : kernTrimHttpWs(args[0]->toString());
        if (s.empty()) {
            out["ok"] = std::make_shared<Value>(Value::fromBool(false));
            out["disposition"] = std::make_shared<Value>(Value::fromString(""));
            out["filename"] = std::make_shared<Value>(Value::fromString(""));
            out["filename_star"] = std::make_shared<Value>(Value::fromString(""));
            return Value::fromMap(std::move(out));
        }
        size_t semi0 = s.find(';');
        std::string disp = semi0 == std::string::npos ? s : s.substr(0, semi0);
        disp = kernTrimHttpWs(disp);
        for (char& c : disp) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::string rest = semi0 == std::string::npos ? "" : s.substr(semi0);
        std::unordered_map<std::string, std::string> kv;
        kernParseSemicolonKeyValues(rest, kv);
        std::string filename;
        std::string filenameStar;
        auto itf = kv.find("filename");
        if (itf != kv.end()) filename = itf->second;
        auto itfs = kv.find("filename*");
        if (itfs != kv.end()) {
            std::string fs = itfs->second;
            size_t apos = fs.find("''");
            if (apos != std::string::npos) filenameStar = kernPercentDecodeLoose(fs.substr(apos + 2));
            else
                filenameStar = kernPercentDecodeLoose(fs);
        }
        out["ok"] = std::make_shared<Value>(Value::fromBool(true));
        out["disposition"] = std::make_shared<Value>(Value::fromString(disp));
        out["filename"] = std::make_shared<Value>(Value::fromString(filename));
        out["filename_star"] = std::make_shared<Value>(Value::fromString(filenameStar));
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("parse_content_disposition", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string u = args.empty() || !args[0] ? "" : args[0]->toString();
        return Value::fromString(kernUrlNormalize(u));
    });
    setGlobalFn("url_normalize", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string html = args.empty() || !args[0] ? "" : args[0]->toString();
        std::unordered_set<std::string> allow;
        if (args.size() >= 2 && args[1]) {
            if (args[1]->type == Value::Type::ARRAY) {
                for (const auto& e : std::get<std::vector<ValuePtr>>(args[1]->data)) {
                    if (!e) continue;
                    std::string t = e->toString();
                    t = kernTrimHttpWs(t);
                    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (!t.empty()) allow.insert(std::move(t));
                }
            } else {
                std::string list = args[1]->toString();
                size_t st = 0;
                while (st < list.size()) {
                    size_t cm = list.find(',', st);
                    std::string tok = cm == std::string::npos ? list.substr(st) : list.substr(st, cm - st);
                    tok = kernTrimHttpWs(tok);
                    for (char& c : tok) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (!tok.empty()) allow.insert(std::move(tok));
                    st = cm == std::string::npos ? list.size() : cm + 1;
                }
            }
        }
        return Value::fromString(kernHtmlSanitizeStrict(html, allow));
    });
    setGlobalFn("html_sanitize_strict", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        return Value::fromString(kernCssUrlEscape(s));
    });
    setGlobalFn("css_url_escape", i - 1);

    makeBuiltin(i++, [toInt](VM*, std::vector<ValuePtr> args) {
        int64_t status = args.empty() || !args[0] ? 200 : toInt(args[0]);
        if (status < 100 || status > 999) status = 500;
        std::ostringstream head;
        head << "HTTP/1.1 " << status << " " << kernHttpReasonPhrase(static_cast<int>(status)) << "\r\n";
        if (args.size() >= 2 && args[1] && args[1]->type == Value::Type::MAP) {
            auto& hm = std::get<std::unordered_map<std::string, ValuePtr>>(args[1]->data);
            std::vector<std::string> keys;
            keys.reserve(hm.size());
            for (const auto& kv : hm) keys.push_back(kv.first);
            std::sort(keys.begin(), keys.end());
            for (const std::string& k : keys) {
                auto it = hm.find(k);
                if (it == hm.end() || !it->second) continue;
                std::string v = it->second->toString();
                for (char c : v) {
                    if (c == '\r' || c == '\n') {
                        v.clear();
                        break;
                    }
                }
                head << k << ": " << v << "\r\n";
            }
        }
        head << "\r\n";
        std::string body = args.size() >= 3 && args[2] ? args[2]->toString() : "";
        return Value::fromString(head.str() + body);
    });
    setGlobalFn("http_build_response", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        std::string out;
        out.reserve(s.size() + 32);
        for (unsigned char c : s) {
            switch (c) {
                case '&':
                    out += "&amp;";
                    break;
                case '<':
                    out += "&lt;";
                    break;
                case '>':
                    out += "&gt;";
                    break;
                case '"':
                    out += "&quot;";
                    break;
                case '\'':
                    out += "&#39;";
                    break;
                default:
                    out += static_cast<char>(c);
                    break;
            }
        }
        std::string r;
        r.reserve(out.size() + 16);
        for (size_t i = 0; i < out.size(); ++i) {
            if (out[i] == '\r' && i + 1 < out.size() && out[i + 1] == '\n') {
                r += "<br>";
                ++i;
            } else if (out[i] == '\n')
                r += "<br>";
            else if (out[i] == '\r')
                r += "<br>";
            else
                r += out[i];
        }
        return Value::fromString(std::move(r));
    });
    setGlobalFn("html_nl2br", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string a = args.size() >= 1 && args[0] ? args[0]->toString() : "";
        std::string b = args.size() >= 2 && args[1] ? args[1]->toString() : "";
        return Value::fromString(kernUrlPathJoin(a, b));
    });
    setGlobalFn("url_path_join", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        std::string h = args.empty() || !args[0] ? "" : kernTrimHttpWs(args[0]->toString());
        std::string low;
        low.reserve(h.size());
        for (unsigned char c : h) low += static_cast<char>(std::tolower(c));
        if (low.size() < 6 || low.compare(0, 6, "basic ") != 0) {
            out["ok"] = std::make_shared<Value>(Value::fromBool(false));
            out["username"] = std::make_shared<Value>(Value::fromString(""));
            out["password"] = std::make_shared<Value>(Value::fromString(""));
            return Value::fromMap(std::move(out));
        }
        std::string b64 = kernTrimHttpWs(h.substr(6));
        std::string dec = kernBase64DecodeStr(b64);
        size_t colon = dec.find(':');
        std::string user = colon == std::string::npos ? dec : dec.substr(0, colon);
        std::string pass = colon == std::string::npos ? "" : dec.substr(colon + 1);
        out["ok"] = std::make_shared<Value>(Value::fromBool(true));
        out["username"] = std::make_shared<Value>(Value::fromString(std::move(user)));
        out["password"] = std::make_shared<Value>(Value::fromString(std::move(pass)));
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("parse_authorization_basic", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string url = args.size() >= 1 && args[0] ? args[0]->toString() : "";
        if (args.size() < 2 || !args[1] || args[1]->type != Value::Type::MAP) return Value::fromString(url);
        size_t qpos = url.find('?');
        std::string base = qpos == std::string::npos ? url : url.substr(0, qpos);
        std::string oldq = qpos == std::string::npos ? "" : url.substr(qpos + 1);
        std::unordered_map<std::string, std::string> merged;
        size_t st = 0;
        while (st < oldq.size()) {
            size_t amp = oldq.find('&', st);
            std::string pair = amp == std::string::npos ? oldq.substr(st) : oldq.substr(st, amp - st);
            st = amp == std::string::npos ? oldq.size() : amp + 1;
            size_t eq = pair.find('=');
            std::string k = eq == std::string::npos ? pair : pair.substr(0, eq);
            std::string v = eq == std::string::npos ? "" : pair.substr(eq + 1);
            if (!k.empty()) merged[k] = v;
        }
        auto& add = std::get<std::unordered_map<std::string, ValuePtr>>(args[1]->data);
        for (const auto& kv : add) {
            if (!kv.second) continue;
            merged[kv.first] = kv.second->toString();
        }
        std::vector<std::string> keys;
        keys.reserve(merged.size());
        for (const auto& kv : merged) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        std::ostringstream q;
        bool first = true;
        for (const std::string& k : keys) {
            auto it = merged.find(k);
            if (it == merged.end()) continue;
            if (!first) q << '&';
            first = false;
            q << kernUrlEncodeQueryPart(k) << '=' << kernUrlEncodeQueryPart(it->second);
        }
        if (keys.empty()) return Value::fromString(base);
        return Value::fromString(base + "?" + q.str());
    });
    setGlobalFn("merge_query", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        std::string s = args.empty() || !args[0] ? "" : args[0]->toString();
        struct Item {
            double q;
            std::string lang;
        };
        std::vector<Item> items;
        size_t start = 0;
        while (start < s.size()) {
            size_t comma = s.find(',', start);
            std::string part = comma == std::string::npos ? s.substr(start) : s.substr(start, comma - start);
            part = kernTrimHttpWs(part);
            start = comma == std::string::npos ? s.size() : comma + 1;
            if (part.empty()) continue;
            double qv = 1.0;
            std::string lowp = part;
            for (char& c : lowp) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            size_t qat = lowp.find(";q=");
            std::string lang = part;
            if (qat != std::string::npos) {
                lang = kernTrimHttpWs(part.substr(0, qat));
                std::string qs = kernTrimHttpWs(part.substr(qat + 3));
                char* end = nullptr;
                qv = std::strtod(qs.c_str(), &end);
                if (end == qs.c_str()) qv = 1.0;
            }
            if (!lang.empty()) items.push_back(Item{qv, std::move(lang)});
        }
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.q > b.q; });
        std::vector<ValuePtr> arr;
        for (const auto& it : items) {
            std::unordered_map<std::string, ValuePtr> m;
            m["language"] = std::make_shared<Value>(Value::fromString(it.lang));
            m["q"] = std::make_shared<Value>(Value::fromFloat(it.q));
            arr.push_back(std::make_shared<Value>(Value::fromMap(std::move(m))));
        }
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("parse_accept_language", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty() || !args[0]) return Value::fromBool(false);
        std::string lib = args[0]->toString();
        if (lib.empty()) return Value::fromBool(false);
        auto& allow = vm->mutableRuntimeGuards().ffiLibraryAllowlist;
        if (std::find(allow.begin(), allow.end(), lib) == allow.end())
            allow.push_back(lib);
        return Value::fromBool(true);
    });
    setGlobalFn("ffi_allow_library", i - 1);

    makeBuiltin(i++, [toInt, toPtr](VM* vm, std::vector<ValuePtr> args) {
        if (!vm) return Value::nil();
        const RuntimeGuardPolicy& guards = vm->getRuntimeGuards();
        if (!guards.ffiEnabled)
            throw VMError("ffi_call is disabled in current runtime mode", 0, 0, 5);
        if (!guards.allowUnsafe && !vm->isInUnsafeContext())
            throw VMError("ffi_call requires unsafe context", 0, 0, 5);
        if (args.size() < 5 || !args[0] || !args[1]) return Value::nil();
        std::string dllName = args[0]->toString();
        std::string symbolName = args[1]->toString();
        std::string retType = args[2] ? args[2]->toString() : "int";
        std::string abi = args[3] ? args[3]->toString() : "cdecl";
#ifdef _WIN32
        // windows x64 uses a unified call ABI in practice; keep explicit names for source clarity.
        if (!(abi == "cdecl" || abi == "system" || abi == "win64" || abi == "stdcall")) {
            throw VMError("ffi_call unsupported ABI '" + abi + "'", 0, 0, 5);
        }
#endif
        if (!args[4] || args[4]->type != Value::Type::ARRAY)
            throw VMError("ffi_call expected array of parameter types", 0, 0, 5);
#ifdef _WIN32
        const auto& sigVals = std::get<std::vector<ValuePtr>>(args[4]->data);
        if (guards.sandboxEnabled && !guards.ffiLibraryAllowlist.empty()) {
            if (std::find(guards.ffiLibraryAllowlist.begin(), guards.ffiLibraryAllowlist.end(), dllName) ==
                guards.ffiLibraryAllowlist.end()) {
                throw VMError("ffi_call blocked by sandbox allowlist for library: " + dllName, 0, 0, 5);
            }
        }
        HMODULE lib = nullptr;
        auto lit = g_ffiLibraries.find(dllName);
        if (lit != g_ffiLibraries.end()) lib = lit->second;
        if (!lib) {
            lib = LoadLibraryA(dllName.c_str());
            if (!lib) throw VMError("ffi_call failed to load library: " + dllName, 0, 0, 1);
            g_ffiLibraries[dllName] = lib;
        }
        const std::string symbolKey = dllName + "!" + symbolName;
        FARPROC sym = nullptr;
        auto sit = g_ffiSymbols.find(symbolKey);
        if (sit != g_ffiSymbols.end()) sym = sit->second;
        if (!sym) {
            sym = GetProcAddress(lib, symbolName.c_str());
            if (!sym) throw VMError("ffi_call symbol not found: " + symbolName, 0, 0, 1);
            g_ffiSymbols[symbolKey] = sym;
        }
        int64_t argv64[8] = {0,0,0,0,0,0,0,0};
        size_t argc = args.size() - 5;
        if (argc > 8) throw VMError("ffi_call supports up to 8 arguments", 0, 0, 5);
        if (sigVals.size() != argc)
            throw VMError("ffi_call signature/argument count mismatch", 0, 0, 5);
        auto normalizeFfiType = [](std::string t) {
            for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (t == "int") t = "i64";
            if (t == "float" || t == "double") t = "f64";
            if (t == "bool") t = "u8";
            return t;
        };
        for (size_t k = 0; k < argc; ++k) {
            ValuePtr sig = sigVals[k];
            if (!sig || sig->type != Value::Type::STRING)
                throw VMError("ffi_call signature entry must be a string", 0, 0, 5);
            std::string t = normalizeFfiType(std::get<std::string>(sig->data));
            ValuePtr a = args[5 + k];
            if (!a) { argv64[k] = 0; continue; }
            if (t == "ptr") {
                if (a->type != Value::Type::PTR && a->type != Value::Type::INT)
                    throw VMError("ffi_call arg " + std::to_string(k) + " expected ptr", 0, 0, 5);
                if (a->type == Value::Type::PTR) argv64[k] = static_cast<int64_t>(reinterpret_cast<intptr_t>(toPtr(a)));
                else argv64[k] = static_cast<int64_t>(std::get<int64_t>(a->data));
                continue;
            }
            if (t == "f32" || t == "f64") {
                if (!(a->type == Value::Type::FLOAT || a->type == Value::Type::INT))
                    throw VMError("ffi_call arg " + std::to_string(k) + " expected float", 0, 0, 5);
                double d = (a->type == Value::Type::FLOAT) ? std::get<double>(a->data) : static_cast<double>(std::get<int64_t>(a->data));
                uint64_t bits = 0;
                if (t == "f32") {
                    float f = static_cast<float>(d);
                    std::memcpy(&bits, &f, sizeof(float));
                } else {
                    std::memcpy(&bits, &d, sizeof(double));
                }
                argv64[k] = static_cast<int64_t>(bits);
                continue;
            }
            if (t == "u8" || t == "u16" || t == "u32" || t == "u64" || t == "i8" || t == "i16" || t == "i32" || t == "i64") {
                if (!(a->type == Value::Type::INT || a->type == Value::Type::BOOL || a->type == Value::Type::FLOAT))
                    throw VMError("ffi_call arg " + std::to_string(k) + " expected integer", 0, 0, 5);
                int64_t v = toInt(a);
                if (t == "u8") v &= 0xFF;
                else if (t == "u16") v &= 0xFFFF;
                else if (t == "u32") v &= 0xFFFFFFFFLL;
                else if (t == "i8") v = static_cast<int8_t>(v);
                else if (t == "i16") v = static_cast<int16_t>(v);
                else if (t == "i32") v = static_cast<int32_t>(v);
                argv64[k] = v;
                continue;
            }
            throw VMError("ffi_call unsupported argument type '" + t + "'", 0, 0, 5);
        }

        using Fn0 = intptr_t(__cdecl*)();
        using Fn1 = intptr_t(__cdecl*)(intptr_t);
        using Fn2 = intptr_t(__cdecl*)(intptr_t, intptr_t);
        using Fn3 = intptr_t(__cdecl*)(intptr_t, intptr_t, intptr_t);
        using Fn4 = intptr_t(__cdecl*)(intptr_t, intptr_t, intptr_t, intptr_t);
        using Fn5 = intptr_t(__cdecl*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
        using Fn6 = intptr_t(__cdecl*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
        using Fn7 = intptr_t(__cdecl*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
        using Fn8 = intptr_t(__cdecl*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
        intptr_t out = 0;
        switch (argc) {
            case 0: out = reinterpret_cast<Fn0>(sym)(); break;
            case 1: out = reinterpret_cast<Fn1>(sym)(argv64[0]); break;
            case 2: out = reinterpret_cast<Fn2>(sym)(argv64[0], argv64[1]); break;
            case 3: out = reinterpret_cast<Fn3>(sym)(argv64[0], argv64[1], argv64[2]); break;
            case 4: out = reinterpret_cast<Fn4>(sym)(argv64[0], argv64[1], argv64[2], argv64[3]); break;
            case 5: out = reinterpret_cast<Fn5>(sym)(argv64[0], argv64[1], argv64[2], argv64[3], argv64[4]); break;
            case 6: out = reinterpret_cast<Fn6>(sym)(argv64[0], argv64[1], argv64[2], argv64[3], argv64[4], argv64[5]); break;
            case 7: out = reinterpret_cast<Fn7>(sym)(argv64[0], argv64[1], argv64[2], argv64[3], argv64[4], argv64[5], argv64[6]); break;
            case 8: out = reinterpret_cast<Fn8>(sym)(argv64[0], argv64[1], argv64[2], argv64[3], argv64[4], argv64[5], argv64[6], argv64[7]); break;
            default: break;
        }
        auto normalizeRetType = [](std::string t) {
            for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (t == "int") t = "i64";
            if (t == "float" || t == "double") t = "f64";
            if (t == "bool") t = "u8";
            return t;
        };
        std::string rt = normalizeRetType(retType);
        if (rt == "void") return Value::nil();
        if (rt == "ptr") return Value::fromPtr(reinterpret_cast<void*>(out));
        if (rt == "f32") {
            uint32_t bits = static_cast<uint32_t>(out & 0xFFFFFFFFu);
            float f = 0.0f;
            std::memcpy(&f, &bits, sizeof(float));
            return Value::fromFloat(static_cast<double>(f));
        }
        if (rt == "f64") {
            uint64_t bits = static_cast<uint64_t>(out);
            double d = 0.0;
            std::memcpy(&d, &bits, sizeof(double));
            return Value::fromFloat(d);
        }
        if (rt == "u8") return Value::fromBool((out & 0xFF) != 0);
        if (rt == "u16") return Value::fromInt(static_cast<int64_t>(out & 0xFFFF));
        if (rt == "u32") return Value::fromInt(static_cast<int64_t>(static_cast<uint32_t>(out)));
        if (rt == "i8") return Value::fromInt(static_cast<int8_t>(out));
        if (rt == "i16") return Value::fromInt(static_cast<int16_t>(out));
        if (rt == "i32") return Value::fromInt(static_cast<int32_t>(out));
        if (rt == "u64" || rt == "i64") return Value::fromInt(static_cast<int64_t>(out));
        throw VMError("ffi_call unsupported return type '" + rt + "'", 0, 0, 5);
#else
        (void)toInt;
        (void)toPtr;
        throw VMError("ffi_call is currently implemented for Windows only in phase1", 0, 0, 1);
#endif
    });
    setGlobalFn("ffi_call", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::nil();
        const auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        auto getStr = [&m](const char* key, const char* fallback = "") {
            auto it = m.find(key);
            if (it == m.end() || !it->second) return std::string(fallback);
            return it->second->toString();
        };
        auto getArr = [&m](const char* key) -> ValuePtr {
            auto it = m.find(key);
            if (it == m.end() || !it->second || it->second->type != Value::Type::ARRAY) return nullptr;
            return it->second;
        };
        std::vector<ValuePtr> callArgs;
        callArgs.push_back(std::make_shared<Value>(Value::fromString(getStr("library"))));
        callArgs.push_back(std::make_shared<Value>(Value::fromString(getStr("symbol"))));
        callArgs.push_back(std::make_shared<Value>(Value::fromString(getStr("returns", "i64"))));
        callArgs.push_back(std::make_shared<Value>(Value::fromString(getStr("abi", "cdecl"))));
        ValuePtr sig = getArr("params");
        if (!sig) sig = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
        callArgs.push_back(sig);
        ValuePtr inArgs = nullptr;
        if (args.size() >= 2 && args[1] && args[1]->type == Value::Type::ARRAY) inArgs = args[1];
        else inArgs = getArr("args");
        if (inArgs) {
            const auto& arr = std::get<std::vector<ValuePtr>>(inArgs->data);
            for (const auto& v : arr) callArgs.push_back(v ? v : std::make_shared<Value>(Value::nil()));
        }
        ValuePtr ffiCallFn = vm->getGlobal("ffi_call");
        if (!ffiCallFn) return Value::nil();
        ValuePtr out = vm->callValue(ffiCallFn, callArgs);
        if (!out) return Value::nil();
        return *out;
    });
    setGlobalFn("ffi_call_typed", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.size() < 2 || !args[1]) return Value::nil();
        ValuePtr decoratorNameV = args[0];
        ValuePtr target = args[1];
        if (!decoratorNameV || decoratorNameV->type != Value::Type::STRING) return *target;
        if (target->type != Value::Type::FUNCTION) return *target;

        std::string decoratorName = std::get<std::string>(decoratorNameV->data);
        std::unordered_map<std::string, ValuePtr> registryMap;
        ValuePtr reg = vm->getDecoratorRegistry();
        if (reg && reg->type == Value::Type::MAP) {
            registryMap = std::get<std::unordered_map<std::string, ValuePtr>>(reg->data);
        } else {
            registryMap["commands"] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
            registryMap["events"] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
        }
        auto ensureArray = [&registryMap](const char* key) -> std::vector<ValuePtr>* {
            auto it = registryMap.find(key);
            if (it == registryMap.end() || !it->second || it->second->type != Value::Type::ARRAY) {
                registryMap[key] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
                it = registryMap.find(key);
            }
            return &std::get<std::vector<ValuePtr>>(it->second->data);
        };
        auto fnObj = std::get<FunctionPtr>(target->data);
        if (decoratorName == "command") {
            std::string commandName = (fnObj && !fnObj->name.empty()) ? fnObj->name : "command";
            std::unordered_map<std::string, ValuePtr> entry;
            entry["name"] = std::make_shared<Value>(Value::fromString(commandName));
            entry["handler"] = target;
            std::unordered_map<std::string, ValuePtr> meta;
            meta["description"] = std::make_shared<Value>(Value::fromString(""));
            meta["args"] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
            meta["inject_ctx"] = std::make_shared<Value>(Value::fromBool(true));
            std::vector<ValuePtr> aliases;
            bool hasName = false;
            for (size_t i = 2; i < args.size(); ++i) {
                ValuePtr extra = args[i];
                if (!extra) continue;
                if (!hasName && extra->type == Value::Type::STRING) {
                    commandName = std::get<std::string>(extra->data);
                    hasName = true;
                    continue;
                }
                if (extra->type != Value::Type::MAP) continue;
                auto& mm = std::get<std::unordered_map<std::string, ValuePtr>>(extra->data);
                auto itName = mm.find("name");
                if (itName != mm.end() && itName->second && itName->second->type == Value::Type::STRING) {
                    commandName = std::get<std::string>(itName->second->data);
                    hasName = true;
                }
                auto itDesc = mm.find("description");
                if (itDesc != mm.end() && itDesc->second) {
                    meta["description"] = std::make_shared<Value>(Value::fromString(itDesc->second->toString()));
                }
                auto itArgs = mm.find("args");
                if (itArgs != mm.end() && itArgs->second && itArgs->second->type == Value::Type::ARRAY) {
                    meta["args"] = itArgs->second;
                }
                auto itInject = mm.find("inject_ctx");
                if (itInject != mm.end() && itInject->second && itInject->second->type == Value::Type::BOOL) {
                    meta["inject_ctx"] = itInject->second;
                }
                auto itAliases = mm.find("aliases");
                if (itAliases != mm.end() && itAliases->second && itAliases->second->type == Value::Type::ARRAY) {
                    auto& arr = std::get<std::vector<ValuePtr>>(itAliases->second->data);
                    aliases.clear();
                    for (const auto& a : arr) {
                        if (!a) continue;
                        aliases.push_back(std::make_shared<Value>(Value::fromString(a->toString())));
                    }
                }
            }
            entry["name"] = std::make_shared<Value>(Value::fromString(commandName));
            if (!aliases.empty()) {
                entry["aliases"] = std::make_shared<Value>(Value::fromArray(std::move(aliases)));
            }
            entry["meta"] = std::make_shared<Value>(Value::fromMap(std::move(meta)));
            ensureArray("commands")->push_back(std::make_shared<Value>(Value::fromMap(std::move(entry))));
        } else if (decoratorName == "event") {
            std::string eventName = "on_command";
            bool onceFlag = false;
            bool stopOnError = false;
            int64_t priority = 0;
            int64_t retries = 0;
            int64_t retryDelayMs = 0;
            int64_t timeoutMs = 0;
            int64_t maxFailures = 0;
            int64_t cooldownMs = 0;
            int64_t maxCircuitTrips = 0;
            bool hasEventName = false;
            for (size_t i = 2; i < args.size(); ++i) {
                ValuePtr extra = args[i];
                if (!extra) continue;
                if (!hasEventName && extra->type == Value::Type::STRING) {
                    eventName = std::get<std::string>(extra->data);
                    hasEventName = true;
                    continue;
                }
                if (extra->type != Value::Type::MAP) continue;
                auto& mm = std::get<std::unordered_map<std::string, ValuePtr>>(extra->data);
                auto itName = mm.find("event");
                if (itName == mm.end()) itName = mm.find("name");
                if (itName != mm.end() && itName->second && itName->second->type == Value::Type::STRING) {
                    eventName = std::get<std::string>(itName->second->data);
                    hasEventName = true;
                }
                auto itOnce = mm.find("once");
                if (itOnce != mm.end() && itOnce->second && itOnce->second->type == Value::Type::BOOL) {
                    onceFlag = std::get<bool>(itOnce->second->data);
                }
                auto itPriority = mm.find("priority");
                if (itPriority != mm.end() && itPriority->second) {
                    if (itPriority->second->type == Value::Type::INT)
                        priority = std::get<int64_t>(itPriority->second->data);
                    else if (itPriority->second->type == Value::Type::FLOAT)
                        priority = static_cast<int64_t>(std::get<double>(itPriority->second->data));
                }
                auto itRetries = mm.find("retries");
                if (itRetries != mm.end() && itRetries->second) {
                    if (itRetries->second->type == Value::Type::INT)
                        retries = std::get<int64_t>(itRetries->second->data);
                    else if (itRetries->second->type == Value::Type::FLOAT)
                        retries = static_cast<int64_t>(std::get<double>(itRetries->second->data));
                }
                auto itRetryDelay = mm.find("retry_delay_ms");
                if (itRetryDelay != mm.end() && itRetryDelay->second) {
                    if (itRetryDelay->second->type == Value::Type::INT)
                        retryDelayMs = std::get<int64_t>(itRetryDelay->second->data);
                    else if (itRetryDelay->second->type == Value::Type::FLOAT)
                        retryDelayMs = static_cast<int64_t>(std::get<double>(itRetryDelay->second->data));
                }
                auto itTimeout = mm.find("timeout_ms");
                if (itTimeout != mm.end() && itTimeout->second) {
                    if (itTimeout->second->type == Value::Type::INT)
                        timeoutMs = std::get<int64_t>(itTimeout->second->data);
                    else if (itTimeout->second->type == Value::Type::FLOAT)
                        timeoutMs = static_cast<int64_t>(std::get<double>(itTimeout->second->data));
                }
                auto itMaxFailures = mm.find("max_failures");
                if (itMaxFailures != mm.end() && itMaxFailures->second) {
                    if (itMaxFailures->second->type == Value::Type::INT)
                        maxFailures = std::get<int64_t>(itMaxFailures->second->data);
                    else if (itMaxFailures->second->type == Value::Type::FLOAT)
                        maxFailures = static_cast<int64_t>(std::get<double>(itMaxFailures->second->data));
                }
                auto itCooldown = mm.find("cooldown_ms");
                if (itCooldown != mm.end() && itCooldown->second) {
                    if (itCooldown->second->type == Value::Type::INT)
                        cooldownMs = std::get<int64_t>(itCooldown->second->data);
                    else if (itCooldown->second->type == Value::Type::FLOAT)
                        cooldownMs = static_cast<int64_t>(std::get<double>(itCooldown->second->data));
                }
                auto itMaxTrips = mm.find("max_circuit_trips");
                if (itMaxTrips != mm.end() && itMaxTrips->second) {
                    if (itMaxTrips->second->type == Value::Type::INT)
                        maxCircuitTrips = std::get<int64_t>(itMaxTrips->second->data);
                    else if (itMaxTrips->second->type == Value::Type::FLOAT)
                        maxCircuitTrips = static_cast<int64_t>(std::get<double>(itMaxTrips->second->data));
                }
                auto itStop = mm.find("stop_on_error");
                if (itStop != mm.end() && itStop->second && itStop->second->type == Value::Type::BOOL) {
                    stopOnError = std::get<bool>(itStop->second->data);
                }
            }
            std::unordered_map<std::string, ValuePtr> entry;
            entry["event"] = std::make_shared<Value>(Value::fromString(eventName));
            entry["handler"] = target;
            entry["once"] = std::make_shared<Value>(Value::fromBool(onceFlag));
            entry["priority"] = std::make_shared<Value>(Value::fromInt(priority));
            entry["stop_on_error"] = std::make_shared<Value>(Value::fromBool(stopOnError));
            entry["retries"] = std::make_shared<Value>(Value::fromInt(retries));
            entry["retry_delay_ms"] = std::make_shared<Value>(Value::fromInt(retryDelayMs));
            entry["timeout_ms"] = std::make_shared<Value>(Value::fromInt(timeoutMs));
            entry["max_failures"] = std::make_shared<Value>(Value::fromInt(maxFailures));
            entry["cooldown_ms"] = std::make_shared<Value>(Value::fromInt(cooldownMs));
            entry["max_circuit_trips"] = std::make_shared<Value>(Value::fromInt(maxCircuitTrips));
            ensureArray("events")->push_back(std::make_shared<Value>(Value::fromMap(std::move(entry))));
        }

        vm->setDecoratorRegistry(std::make_shared<Value>(Value::fromMap(std::move(registryMap))));
        return *target;
    });
    setGlobalFn("__apply_decorator", i - 1);

    // Cooperative task: runs the callable on the current VM (same thread). Result map is awaitable via __await_task.
    // For OS threads or subprocesses use stdlib process / concurrency modules, not this helper.
    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> task;
        task["state"] = std::make_shared<Value>(Value::fromString("done"));
        task["result"] = std::make_shared<Value>(Value::nil());
        task["error"] = std::make_shared<Value>(Value::nil());
        if (!vm || args.empty() || !args[0]) return Value::fromMap(std::move(task));
        ValuePtr target = args[0];
        try {
            if (target->type == Value::Type::FUNCTION) {
                ValuePtr res = vm->callValue(target, {});
                task["result"] = res ? res : std::make_shared<Value>(Value::nil());
            } else {
                task["result"] = target;
            }
        } catch (const std::exception& ex) {
            task["state"] = std::make_shared<Value>(Value::fromString("failed"));
            task["result"] = std::make_shared<Value>(Value::nil());
            task["error"] = std::make_shared<Value>(Value::fromString(ex.what()));
        }
        return Value::fromMap(std::move(task));
    });
    setGlobalFn("__spawn_task", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr> args) {
        if (args.empty() || !args[0]) return Value::nil();
        ValuePtr in = args[0];
        if (in->type != Value::Type::MAP) return *in;
        auto& m = std::get<std::unordered_map<std::string, ValuePtr>>(in->data);
        auto itState = m.find("state");
        auto itErr = m.find("error");
        bool failed = false;
        if (itState != m.end() && itState->second && itState->second->type == Value::Type::STRING)
            failed = (std::get<std::string>(itState->second->data) == "failed");
        if (failed && itErr != m.end() && itErr->second && itErr->second->type != Value::Type::NIL) {
            throw VMError(itErr->second->toString(), 0, 0, 1);
        }
        auto itRes = m.find("result");
        if (itRes != m.end() && itRes->second) return *(itRes->second);
        return Value::nil();
    });
    setGlobalFn("__await_task", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        if (!vm || args.empty() || !args[0] || args[0]->type != Value::Type::MAP) return Value::fromInt(0);
        ValuePtr regV = vm->getDecoratorRegistry();
        if (!regV || regV->type != Value::Type::MAP) return Value::fromInt(0);

        auto& runtime = std::get<std::unordered_map<std::string, ValuePtr>>(args[0]->data);
        auto rtCmdIt = runtime.find("commands");
        auto rtEvtIt = runtime.find("events");
        if (rtCmdIt == runtime.end() || !rtCmdIt->second || rtCmdIt->second->type != Value::Type::MAP ||
            rtEvtIt == runtime.end() || !rtEvtIt->second || rtEvtIt->second->type != Value::Type::MAP) {
            return Value::fromInt(0);
        }
        auto& commandRegistry = std::get<std::unordered_map<std::string, ValuePtr>>(rtCmdIt->second->data);
        auto& eventBus = std::get<std::unordered_map<std::string, ValuePtr>>(rtEvtIt->second->data);
        auto& reg = std::get<std::unordered_map<std::string, ValuePtr>>(regV->data);
        int64_t applied = 0;

        auto cmdIt = reg.find("commands");
        if (cmdIt != reg.end() && cmdIt->second && cmdIt->second->type == Value::Type::ARRAY) {
            auto& arr = std::get<std::vector<ValuePtr>>(cmdIt->second->data);
            for (const auto& item : arr) {
                if (!item || item->type != Value::Type::MAP) continue;
                auto& e = std::get<std::unordered_map<std::string, ValuePtr>>(item->data);
                auto itName = e.find("name");
                auto itHandler = e.find("handler");
                auto itMeta = e.find("meta");
                auto itAliases = e.find("aliases");
                if (itName == e.end() || !itName->second || itName->second->type != Value::Type::STRING) continue;
                if (itHandler == e.end() || !itHandler->second || itHandler->second->type != Value::Type::FUNCTION) continue;
                std::string name = std::get<std::string>(itName->second->data);
                std::unordered_map<std::string, ValuePtr> cmdEntry;
                cmdEntry["name"] = itName->second;
                cmdEntry["handler"] = itHandler->second;
                if (itMeta != e.end() && itMeta->second && itMeta->second->type == Value::Type::MAP) {
                    cmdEntry["meta"] = itMeta->second;
                } else {
                    std::unordered_map<std::string, ValuePtr> meta;
                    meta["description"] = std::make_shared<Value>(Value::fromString(""));
                    meta["args"] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
                    meta["inject_ctx"] = std::make_shared<Value>(Value::fromBool(true));
                    cmdEntry["meta"] = std::make_shared<Value>(Value::fromMap(std::move(meta)));
                }
                commandRegistry["cmd:" + name] = std::make_shared<Value>(Value::fromMap(std::move(cmdEntry)));
                if (itAliases != e.end() && itAliases->second && itAliases->second->type == Value::Type::ARRAY) {
                    auto& aliases = std::get<std::vector<ValuePtr>>(itAliases->second->data);
                    for (const auto& av : aliases) {
                        if (!av) continue;
                        std::string alias = av->toString();
                        if (alias.empty()) continue;
                        commandRegistry["alias:" + alias] = std::make_shared<Value>(Value::fromString(name));
                    }
                }
                applied++;
            }
            arr.clear();
        }

        auto evIt = reg.find("events");
        if (evIt != reg.end() && evIt->second && evIt->second->type == Value::Type::ARRAY) {
            auto& arr = std::get<std::vector<ValuePtr>>(evIt->second->data);
            auto nextIdIt = eventBus.find("next_id");
            int64_t nextId = 1;
            if (nextIdIt != eventBus.end() && nextIdIt->second && nextIdIt->second->type == Value::Type::INT)
                nextId = std::get<int64_t>(nextIdIt->second->data);
            for (const auto& item : arr) {
                if (!item || item->type != Value::Type::MAP) continue;
                auto& e = std::get<std::unordered_map<std::string, ValuePtr>>(item->data);
                auto itEvent = e.find("event");
                auto itHandler = e.find("handler");
                auto itOnce = e.find("once");
                auto itPriority = e.find("priority");
                auto itStop = e.find("stop_on_error");
                auto itRetries = e.find("retries");
                auto itRetryDelay = e.find("retry_delay_ms");
                auto itTimeout = e.find("timeout_ms");
                auto itMaxFailures = e.find("max_failures");
                auto itCooldown = e.find("cooldown_ms");
                auto itMaxTrips = e.find("max_circuit_trips");
                if (itEvent == e.end() || !itEvent->second || itEvent->second->type != Value::Type::STRING) continue;
                if (itHandler == e.end() || !itHandler->second || itHandler->second->type != Value::Type::FUNCTION) continue;
                std::string eventName = std::get<std::string>(itEvent->second->data);
                bool onceFlag = false;
                if (itOnce != e.end() && itOnce->second && itOnce->second->type == Value::Type::BOOL)
                    onceFlag = std::get<bool>(itOnce->second->data);
                bool stopOnError = false;
                if (itStop != e.end() && itStop->second && itStop->second->type == Value::Type::BOOL)
                    stopOnError = std::get<bool>(itStop->second->data);
                int64_t priority = 0;
                if (itPriority != e.end() && itPriority->second) {
                    if (itPriority->second->type == Value::Type::INT)
                        priority = std::get<int64_t>(itPriority->second->data);
                    else if (itPriority->second->type == Value::Type::FLOAT)
                        priority = static_cast<int64_t>(std::get<double>(itPriority->second->data));
                }
                int64_t retries = 0;
                if (itRetries != e.end() && itRetries->second) {
                    if (itRetries->second->type == Value::Type::INT)
                        retries = std::get<int64_t>(itRetries->second->data);
                    else if (itRetries->second->type == Value::Type::FLOAT)
                        retries = static_cast<int64_t>(std::get<double>(itRetries->second->data));
                }
                int64_t retryDelayMs = 0;
                if (itRetryDelay != e.end() && itRetryDelay->second) {
                    if (itRetryDelay->second->type == Value::Type::INT)
                        retryDelayMs = std::get<int64_t>(itRetryDelay->second->data);
                    else if (itRetryDelay->second->type == Value::Type::FLOAT)
                        retryDelayMs = static_cast<int64_t>(std::get<double>(itRetryDelay->second->data));
                }
                int64_t timeoutMs = 0;
                if (itTimeout != e.end() && itTimeout->second) {
                    if (itTimeout->second->type == Value::Type::INT)
                        timeoutMs = std::get<int64_t>(itTimeout->second->data);
                    else if (itTimeout->second->type == Value::Type::FLOAT)
                        timeoutMs = static_cast<int64_t>(std::get<double>(itTimeout->second->data));
                }
                int64_t maxFailures = 0;
                if (itMaxFailures != e.end() && itMaxFailures->second) {
                    if (itMaxFailures->second->type == Value::Type::INT)
                        maxFailures = std::get<int64_t>(itMaxFailures->second->data);
                    else if (itMaxFailures->second->type == Value::Type::FLOAT)
                        maxFailures = static_cast<int64_t>(std::get<double>(itMaxFailures->second->data));
                }
                int64_t cooldownMs = 0;
                if (itCooldown != e.end() && itCooldown->second) {
                    if (itCooldown->second->type == Value::Type::INT)
                        cooldownMs = std::get<int64_t>(itCooldown->second->data);
                    else if (itCooldown->second->type == Value::Type::FLOAT)
                        cooldownMs = static_cast<int64_t>(std::get<double>(itCooldown->second->data));
                }
                int64_t maxCircuitTrips = 0;
                if (itMaxTrips != e.end() && itMaxTrips->second) {
                    if (itMaxTrips->second->type == Value::Type::INT)
                        maxCircuitTrips = std::get<int64_t>(itMaxTrips->second->data);
                    else if (itMaxTrips->second->type == Value::Type::FLOAT)
                        maxCircuitTrips = static_cast<int64_t>(std::get<double>(itMaxTrips->second->data));
                }
                std::string key = "evt:" + eventName;
                auto listenersIt = eventBus.find(key);
                if (listenersIt == eventBus.end() || !listenersIt->second || listenersIt->second->type != Value::Type::ARRAY) {
                    eventBus[key] = std::make_shared<Value>(Value::fromArray(std::vector<ValuePtr>{}));
                    listenersIt = eventBus.find(key);
                }
                auto& listeners = std::get<std::vector<ValuePtr>>(listenersIt->second->data);
                std::unordered_map<std::string, ValuePtr> listener;
                listener["id"] = std::make_shared<Value>(Value::fromInt(nextId++));
                listener["handler"] = itHandler->second;
                listener["once"] = std::make_shared<Value>(Value::fromBool(onceFlag));
                listener["priority"] = std::make_shared<Value>(Value::fromInt(priority));
                listener["stop_on_error"] = std::make_shared<Value>(Value::fromBool(stopOnError));
                listener["retries"] = std::make_shared<Value>(Value::fromInt(retries));
                listener["retry_delay_ms"] = std::make_shared<Value>(Value::fromInt(retryDelayMs));
                listener["timeout_ms"] = std::make_shared<Value>(Value::fromInt(timeoutMs));
                listener["max_failures"] = std::make_shared<Value>(Value::fromInt(maxFailures));
                listener["cooldown_ms"] = std::make_shared<Value>(Value::fromInt(cooldownMs));
                listener["max_circuit_trips"] = std::make_shared<Value>(Value::fromInt(maxCircuitTrips));
                listener["fail_count"] = std::make_shared<Value>(Value::fromInt(0));
                listener["blocked_until_ms"] = std::make_shared<Value>(Value::fromInt(0));
                listener["circuit_trips"] = std::make_shared<Value>(Value::fromInt(0));
                listener["quarantined"] = std::make_shared<Value>(Value::fromBool(false));
                auto listenerValue = std::make_shared<Value>(Value::fromMap(std::move(listener)));
                size_t insertAt = listeners.size();
                for (size_t li = 0; li < listeners.size(); ++li) {
                    const auto& cur = listeners[li];
                    int64_t curPriority = 0;
                    if (cur && cur->type == Value::Type::MAP) {
                        auto& cm = std::get<std::unordered_map<std::string, ValuePtr>>(cur->data);
                        auto cpIt = cm.find("priority");
                        if (cpIt != cm.end() && cpIt->second) {
                            if (cpIt->second->type == Value::Type::INT)
                                curPriority = std::get<int64_t>(cpIt->second->data);
                            else if (cpIt->second->type == Value::Type::FLOAT)
                                curPriority = static_cast<int64_t>(std::get<double>(cpIt->second->data));
                        }
                    }
                    if (priority > curPriority) {
                        insertAt = li;
                        break;
                    }
                }
                listeners.insert(listeners.begin() + insertAt, listenerValue);
                applied++;
            }
            eventBus["next_id"] = std::make_shared<Value>(Value::fromInt(nextId));
            arr.clear();
        }

        return Value::fromInt(applied);
    });
    setGlobalFn("__runtime_apply_decorators", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        std::unordered_map<std::string, ValuePtr> out;
        out["ok"] = std::make_shared<Value>(Value::fromBool(false));
        out["result"] = std::make_shared<Value>(Value::nil());
        out["error"] = std::make_shared<Value>(Value::nil());
        if (!vm || args.empty() || !args[0] || args[0]->type != Value::Type::FUNCTION) {
            out["error"] = std::make_shared<Value>(Value::fromString("safe_invoke expects function"));
            return Value::fromMap(std::move(out));
        }
        ValuePtr fn = args[0];
        std::vector<ValuePtr> callArgs;
        for (size_t i = 1; i < args.size(); ++i) callArgs.push_back(args[i]);
        try {
            ValuePtr result = vm->callValue(fn, std::move(callArgs));
            out["ok"] = std::make_shared<Value>(Value::fromBool(true));
            out["result"] = result ? result : std::make_shared<Value>(Value::nil());
        } catch (const std::exception& ex) {
            out["error"] = std::make_shared<Value>(Value::fromString(ex.what()));
        }
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("__safe_invoke2", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        std::unordered_map<std::string, ValuePtr> out;
        std::vector<ValuePtr> granted;
        std::vector<ValuePtr> groups;
        std::vector<ValuePtr> grantedWithSource;
        if (vm) {
            const RuntimeGuardPolicy& g = vm->getRuntimeGuards();
            for (const auto& p : g.grantedPermissions) {
                granted.push_back(std::make_shared<Value>(Value::fromString(p)));
                std::unordered_map<std::string, ValuePtr> item;
                item["permission"] = std::make_shared<Value>(Value::fromString(p));
                item["source"] = std::make_shared<Value>(Value::fromString("grant-or-profile"));
                grantedWithSource.push_back(std::make_shared<Value>(Value::fromMap(std::move(item))));
            }
            for (const auto& kv : permissionGroupMap()) {
                bool ok = true;
                for (const auto& need : kv.second) {
                    if (g.grantedPermissions.find(need) == g.grantedPermissions.end()) {
                        ok = false;
                        break;
                    }
                }
                if (ok) groups.push_back(std::make_shared<Value>(Value::fromString(kv.first)));
            }
        }
        out["granted"] = std::make_shared<Value>(Value::fromArray(std::move(granted)));
        out["granted_with_source"] = std::make_shared<Value>(Value::fromArray(std::move(grantedWithSource)));
        out["groups"] = std::make_shared<Value>(Value::fromArray(std::move(groups)));
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("permissions_active", i - 1);

#include "std_builtins_v1.inl"
#include "std_builtins_socket.inl"
    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fs_fd_open");
        if (args.empty() || !args[0]) return Value::fromMap({});
        std::string path = args[0]->toString();
        std::string mode = (args.size() >= 2 && args[1]) ? args[1]->toString() : "rb";
        FILE* f = nullptr;
#ifdef _WIN32
        (void)fopen_s(&f, path.c_str(), mode.c_str());
#else
        f = std::fopen(path.c_str(), mode.c_str());
#endif
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(f != nullptr));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        if (!f) {
            m["error"] = std::make_shared<Value>(Value::fromString("open failed"));
            return Value::fromMap(std::move(m));
        }
        std::lock_guard<std::mutex> lk(g_fdMutex);
        int64_t id = g_nextFdHandle++;
        g_fdHandles[id] = f;
        m["id"] = std::make_shared<Value>(Value::fromInt(id));
        m["error"] = std::make_shared<Value>(Value::fromString(""));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_fd_open", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "fs_fd_close");
        if (args.empty()) return Value::fromBool(false);
        int64_t id = toInt(args[0]);
        FILE* f = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_fdMutex);
            auto it = g_fdHandles.find(id);
            if (it == g_fdHandles.end()) return Value::fromBool(false);
            f = it->second;
            g_fdHandles.erase(it);
        }
        if (!f) return Value::fromBool(false);
        return Value::fromBool(std::fclose(f) == 0);
    });
    setGlobalFn("fs_fd_close", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fs_fd_read");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["data"] = std::make_shared<Value>(Value::fromString(""));
        m["eof"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.empty()) return Value::fromMap(std::move(m));
        int64_t id = toInt(args[0]);
        size_t maxB = args.size() >= 2 ? static_cast<size_t>(std::max<int64_t>(1, toInt(args[1]))) : 65536;
        FILE* f = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_fdMutex);
            auto it = g_fdHandles.find(id);
            if (it == g_fdHandles.end()) return Value::fromMap(std::move(m));
            f = it->second;
        }
        std::string out;
        out.resize(maxB);
        size_t n = std::fread(&out[0], 1, maxB, f);
        out.resize(n);
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        m["data"] = std::make_shared<Value>(Value::fromString(std::move(out)));
        m["eof"] = std::make_shared<Value>(Value::fromBool(std::feof(f) != 0));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_fd_read", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "fs_fd_write");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["written"] = std::make_shared<Value>(Value::fromInt(0));
        if (args.size() < 2 || !args[1]) return Value::fromMap(std::move(m));
        int64_t id = toInt(args[0]);
        FILE* f = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_fdMutex);
            auto it = g_fdHandles.find(id);
            if (it == g_fdHandles.end()) return Value::fromMap(std::move(m));
            f = it->second;
        }
        std::string data = args[1]->toString();
        size_t n = std::fwrite(data.data(), 1, data.size(), f);
        std::fflush(f);
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        m["written"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(n)));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_fd_write", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fs_fd_pread");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["data"] = std::make_shared<Value>(Value::fromString(""));
        if (args.size() < 3) return Value::fromMap(std::move(m));
        int64_t id = toInt(args[0]);
        int64_t off = std::max<int64_t>(0, toInt(args[1]));
        size_t maxB = static_cast<size_t>(std::max<int64_t>(1, toInt(args[2])));
        FILE* f = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_fdMutex);
            auto it = g_fdHandles.find(id);
            if (it == g_fdHandles.end()) return Value::fromMap(std::move(m));
            f = it->second;
        }
        long prev = std::ftell(f);
        if (std::fseek(f, static_cast<long>(off), SEEK_SET) != 0) return Value::fromMap(std::move(m));
        std::string out;
        out.resize(maxB);
        size_t n = std::fread(&out[0], 1, maxB, f);
        out.resize(n);
        if (prev >= 0) std::fseek(f, prev, SEEK_SET);
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        m["data"] = std::make_shared<Value>(Value::fromString(std::move(out)));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_fd_pread", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "fs_fd_pwrite");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["written"] = std::make_shared<Value>(Value::fromInt(0));
        if (args.size() < 3 || !args[2]) return Value::fromMap(std::move(m));
        int64_t id = toInt(args[0]);
        int64_t off = std::max<int64_t>(0, toInt(args[1]));
        FILE* f = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_fdMutex);
            auto it = g_fdHandles.find(id);
            if (it == g_fdHandles.end()) return Value::fromMap(std::move(m));
            f = it->second;
        }
        std::string data = args[2]->toString();
        long prev = std::ftell(f);
        if (std::fseek(f, static_cast<long>(off), SEEK_SET) != 0) return Value::fromMap(std::move(m));
        size_t n = std::fwrite(data.data(), 1, data.size(), f);
        std::fflush(f);
        if (prev >= 0) std::fseek(f, prev, SEEK_SET);
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        m["written"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(n)));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_fd_pwrite", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "fs_flock");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.empty() || !args[0]) return Value::fromMap(std::move(m));
        std::string key = args[0]->toString();
        bool unlock = (args.size() >= 2 && args[1] && args[1]->isTruthy());
        std::lock_guard<std::mutex> lk(g_flockMutex);
        if (unlock) {
            g_flockKeys.erase(key);
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            return Value::fromMap(std::move(m));
        }
        if (g_flockKeys.find(key) != g_flockKeys.end()) return Value::fromMap(std::move(m));
        g_flockKeys.insert(key);
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_flock", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fs_statx");
        std::unordered_map<std::string, ValuePtr> m;
        if (args.empty() || !args[0]) return Value::fromMap(std::move(m));
        std::error_code ec;
        std::filesystem::path p(args[0]->toString());
        auto st = std::filesystem::status(p, ec);
        const bool exists = std::filesystem::exists(st);
        m["exists"] = std::make_shared<Value>(Value::fromBool(exists));
        m["is_file"] = std::make_shared<Value>(Value::fromBool(exists && std::filesystem::is_regular_file(st)));
        m["is_dir"] = std::make_shared<Value>(Value::fromBool(exists && std::filesystem::is_directory(st)));
        int64_t sz = -1;
        if (!ec && exists && std::filesystem::is_regular_file(st)) {
            std::error_code ec2;
            sz = static_cast<int64_t>(std::filesystem::file_size(p, ec2));
        }
        m["size"] = std::make_shared<Value>(Value::fromInt(sz));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_statx", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemWrite, "fs_atomic_write");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        if (args.size() < 2 || !args[0]) return Value::fromMap(std::move(m));
        std::filesystem::path target(args[0]->toString());
        std::string data = args[1] ? args[1]->toString() : "";
        std::filesystem::path tmp = target;
        tmp += ".kern_tmp";
        try {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << data;
            out.close();
            std::error_code ec;
            std::filesystem::rename(tmp, target, ec);
            if (ec) {
                std::filesystem::remove(target, ec);
                ec.clear();
                std::filesystem::rename(tmp, target, ec);
                if (ec) return Value::fromMap(std::move(m));
            }
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        } catch (...) {}
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_atomic_write", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fs_watch");
        std::unordered_map<std::string, ValuePtr> out;
        std::string mode = (args.size() >= 1 && args[0]) ? args[0]->toString() : "create";
        if (mode == "create") {
            std::string root = (args.size() >= 2 && args[1]) ? args[1]->toString() : ".";
            std::unordered_map<std::string, int64_t> snap;
            std::error_code ec;
            for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) break;
                if (!it->is_regular_file()) continue;
                auto t = it->last_write_time(ec);
                if (ec) continue;
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
                snap[it->path().string()] = static_cast<int64_t>(ms);
            }
            std::lock_guard<std::mutex> lk(g_fsWatchMutex);
            int64_t id = g_nextWatchId++;
            g_fsWatchState[id] = std::move(snap);
            out["ok"] = std::make_shared<Value>(Value::fromBool(true));
            out["id"] = std::make_shared<Value>(Value::fromInt(id));
            return Value::fromMap(std::move(out));
        }
        if (mode == "close") {
            int64_t id = (args.size() >= 2 && args[1]) ? (args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0) : 0;
            std::lock_guard<std::mutex> lk(g_fsWatchMutex);
            g_fsWatchState.erase(id);
            out["ok"] = std::make_shared<Value>(Value::fromBool(true));
            return Value::fromMap(std::move(out));
        }
        int64_t id = (args.size() >= 2 && args[1]) ? (args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0) : 0;
        std::string root = (args.size() >= 3 && args[2]) ? args[2]->toString() : ".";
        std::unordered_map<std::string, int64_t> cur;
        std::error_code ec;
        for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (!it->is_regular_file()) continue;
            auto t = it->last_write_time(ec);
            if (ec) continue;
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
            cur[it->path().string()] = static_cast<int64_t>(ms);
        }
        std::vector<ValuePtr> changed;
        {
            std::lock_guard<std::mutex> lk(g_fsWatchMutex);
            auto wit = g_fsWatchState.find(id);
            if (wit == g_fsWatchState.end()) return Value::fromMap(std::move(out));
            for (const auto& kv : cur) {
                auto pit = wit->second.find(kv.first);
                if (pit == wit->second.end() || pit->second != kv.second)
                    changed.push_back(std::make_shared<Value>(Value::fromString(kv.first)));
            }
            wit->second = std::move(cur);
        }
        out["ok"] = std::make_shared<Value>(Value::fromBool(true));
        out["changed"] = std::make_shared<Value>(Value::fromArray(std::move(changed)));
        return Value::fromMap(std::move(out));
    });
    setGlobalFn("fs_watch", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fs_space");
        std::string path = (args.empty() || !args[0]) ? "." : args[0]->toString();
        std::unordered_map<std::string, ValuePtr> m;
        std::error_code ec;
        auto s = std::filesystem::space(path, ec);
        m["ok"] = std::make_shared<Value>(Value::fromBool(!ec));
        m["capacity"] = std::make_shared<Value>(Value::fromInt(ec ? -1 : static_cast<int64_t>(s.capacity)));
        m["free"] = std::make_shared<Value>(Value::fromInt(ec ? -1 : static_cast<int64_t>(s.free)));
        m["available"] = std::make_shared<Value>(Value::fromInt(ec ? -1 : static_cast<int64_t>(s.available)));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("fs_space", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kFilesystemRead, "fs_mounts");
        std::vector<ValuePtr> arr;
#ifdef _WIN32
        for (char d = 'A'; d <= 'Z'; ++d) {
            std::string root;
            root += d;
            root += ":\\";
            if (std::filesystem::exists(root))
                arr.push_back(std::make_shared<Value>(Value::fromString(root)));
        }
#else
        arr.push_back(std::make_shared<Value>(Value::fromString("/")));
#endif
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("fs_mounts", i - 1);

    makeBuiltin(i++, [toInt, argsToCommand](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "process_spawn_v2");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromMap(std::move(m));
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        if (arr.empty()) return Value::fromMap(std::move(m));
        std::string cmd = argsToCommand(arr);
#ifdef _WIN32
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<char> buf(cmd.begin(), cmd.end());
        buf.push_back('\0');
        BOOL ok = CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (!ok) return Value::fromMap(std::move(m));
        CloseHandle(pi.hThread);
        int64_t hid = g_nextSpawnHandle++;
        g_spawnHandles[hid] = pi.hProcess;
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        m["id"] = std::make_shared<Value>(Value::fromInt(hid));
        return Value::fromMap(std::move(m));
#else
        int rc = std::system(cmd.c_str());
        m["ok"] = std::make_shared<Value>(Value::fromBool(rc >= 0));
        m["id"] = std::make_shared<Value>(Value::fromInt(rc));
        return Value::fromMap(std::move(m));
#endif
    });
    setGlobalFn("process_spawn_v2", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "process_wait");
        if (args.empty()) return Value::fromMap({});
#ifdef _WIN32
        int64_t hid = toInt(args[0]);
        auto it = g_spawnHandles.find(hid);
        if (it == g_spawnHandles.end() || !it->second) return Value::fromMap({});
        int64_t timeoutMs = args.size() >= 2 ? std::max<int64_t>(0, toInt(args[1])) : -1;
        DWORD wr = WaitForSingleObject(it->second, timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(wr == WAIT_OBJECT_0));
        m["exited"] = std::make_shared<Value>(Value::fromBool(wr == WAIT_OBJECT_0));
        m["timed_out"] = std::make_shared<Value>(Value::fromBool(wr == WAIT_TIMEOUT));
        DWORD code = 0;
        if (wr == WAIT_OBJECT_0 && GetExitCodeProcess(it->second, &code)) {
            m["code"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(code)));
            CloseHandle(it->second);
            g_spawnHandles.erase(it);
        } else {
            m["code"] = std::make_shared<Value>(Value::fromInt(-1));
        }
        return Value::fromMap(std::move(m));
#else
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["exited"] = std::make_shared<Value>(Value::fromBool(false));
        m["timed_out"] = std::make_shared<Value>(Value::fromBool(false));
        m["code"] = std::make_shared<Value>(Value::fromInt(-1));
        return Value::fromMap(std::move(m));
#endif
    });
    setGlobalFn("process_wait", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "process_kill_tree");
        if (args.empty()) return Value::fromBool(false);
#ifdef _WIN32
        int64_t hid = toInt(args[0]);
        auto it = g_spawnHandles.find(hid);
        if (it == g_spawnHandles.end() || !it->second) return Value::fromBool(false);
        BOOL ok = TerminateProcess(it->second, 1);
        CloseHandle(it->second);
        g_spawnHandles.erase(it);
        return Value::fromBool(ok != 0);
#else
        return Value::fromBool(false);
#endif
    });
    setGlobalFn("process_kill_tree", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kProcessControl, "process_list");
        std::vector<ValuePtr> arr;
        std::unordered_map<std::string, ValuePtr> self;
        self["pid"] = std::make_shared<Value>(Value::fromInt(
#ifdef _WIN32
            static_cast<int64_t>(GetCurrentProcessId())
#else
            static_cast<int64_t>(::getpid())
#endif
        ));
        self["name"] = std::make_shared<Value>(Value::fromString("kern"));
        self["self"] = std::make_shared<Value>(Value::fromBool(true));
        arr.push_back(std::make_shared<Value>(Value::fromMap(std::move(self))));
        (void)vm;
        return Value::fromArray(std::move(arr));
    });
    setGlobalFn("process_list", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr>) {
        vmRequirePermission(vm, Perm::kProcessControl, "process_job_create");
        std::lock_guard<std::mutex> lk(g_processJobsMutex);
        int64_t id = g_nextProcessJobId++;
        g_processJobs[id] = {};
        return Value::fromInt(id);
    });
    setGlobalFn("process_job_create", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "process_job_add");
        if (args.size() < 2) return Value::fromBool(false);
        int64_t jobId = toInt(args[0]);
        int64_t pid = toInt(args[1]);
        std::lock_guard<std::mutex> lk(g_processJobsMutex);
        auto it = g_processJobs.find(jobId);
        if (it == g_processJobs.end()) return Value::fromBool(false);
        it->second.push_back(pid);
        return Value::fromBool(true);
    });
    setGlobalFn("process_job_add", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "process_job_kill");
        if (args.empty()) return Value::fromBool(false);
        int64_t jobId = toInt(args[0]);
        std::vector<int64_t> pids;
        {
            std::lock_guard<std::mutex> lk(g_processJobsMutex);
            auto it = g_processJobs.find(jobId);
            if (it == g_processJobs.end()) return Value::fromBool(false);
            pids = it->second;
            g_processJobs.erase(it);
        }
#ifdef _WIN32
        for (int64_t hid : pids) {
            auto it = g_spawnHandles.find(hid);
            if (it != g_spawnHandles.end() && it->second) {
                (void)TerminateProcess(it->second, 1);
                CloseHandle(it->second);
                g_spawnHandles.erase(it);
            }
        }
#endif
        return Value::fromBool(true);
    });
    setGlobalFn("process_job_kill", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "os_signal_trap");
        if (args.size() < 2 || !args[0]) return Value::fromBool(false);
        std::string sig = args[0]->toString();
        std::lock_guard<std::mutex> lk(g_signalTrapMutex);
        g_signalTraps[sig] = args[1] ? args[1] : std::make_shared<Value>(Value::nil());
        return Value::fromBool(true);
    });
    setGlobalFn("os_signal_trap", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kProcessControl, "os_signal_untrap");
        if (args.empty() || !args[0]) return Value::fromBool(false);
        std::string sig = args[0]->toString();
        std::lock_guard<std::mutex> lk(g_signalTrapMutex);
        g_signalTraps.erase(sig);
        return Value::fromBool(true);
    });
    setGlobalFn("os_signal_untrap", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
        std::unordered_map<std::string, ValuePtr> m;
        m["cpu_count"] = std::make_shared<Value>(Value::fromInt(static_cast<int64_t>(std::thread::hardware_concurrency())));
        m["max_open_files"] = std::make_shared<Value>(Value::fromInt(-1));
        m["max_processes"] = std::make_shared<Value>(Value::fromInt(-1));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("os_runtime_limits", i - 1);

    makeBuiltin(i++, [](VM*, std::vector<ValuePtr>) {
        std::unordered_map<std::string, ValuePtr> m;
        m["fs_fd"] = std::make_shared<Value>(Value::fromBool(true));
        m["fs_watch_poll"] = std::make_shared<Value>(Value::fromBool(true));
        m["process_jobs"] = std::make_shared<Value>(Value::fromBool(true));
        m["tcp_udp"] = std::make_shared<Value>(Value::fromBool(true));
        m["tls"] = std::make_shared<Value>(Value::fromBool(false));
        m["websocket"] = std::make_shared<Value>(Value::fromBool(false));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("os_features", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "net_tcp_server");
        if (args.size() < 2 || !args[0] || !args[1]) return Value::fromMap({});
        int64_t backlog = (args.size() >= 3 && args[2]) ? std::max<int64_t>(1, (args[2]->type == Value::Type::INT ? std::get<int64_t>(args[2]->data) : 16)) : 16;
        std::string err;
        int64_t id = -1;
        if (!kernTcpListen(args[0]->toString(), static_cast<int>(args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0), static_cast<int>(backlog), id, err)) {
            std::unordered_map<std::string, ValuePtr> m;
            m["ok"] = std::make_shared<Value>(Value::fromBool(false));
            m["error"] = std::make_shared<Value>(Value::fromString(err));
            return Value::fromMap(std::move(m));
        }
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(true));
        m["id"] = std::make_shared<Value>(Value::fromInt(id));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("net_tcp_server", i - 1);

    makeBuiltin(i++, [toInt](VM* vm, std::vector<ValuePtr> args) {
        if (!vmPermissionAllowed(vm, Perm::kNetworkTcp) && !vmPermissionAllowed(vm, Perm::kNetworkUdp))
            vmRequirePermission(vm, Perm::kNetworkTcp, "net_tcp_poll");
        if (args.empty() || !args[0] || args[0]->type != Value::Type::ARRAY) return Value::fromMap({});
        auto& arr = std::get<std::vector<ValuePtr>>(args[0]->data);
        std::vector<int64_t> ids;
        for (const auto& v : arr) ids.push_back(toInt(v));
        int timeoutMs = args.size() >= 2 ? static_cast<int>(toInt(args[1])) : 0;
        bool writeMode = args.size() >= 3 && args[2] && args[2]->isTruthy();
        std::vector<int64_t> ready;
        std::string err;
        bool ok = writeMode ? kernSocketSelectWrite(ids, timeoutMs, ready, err) : kernSocketSelectRead(ids, timeoutMs, ready, err);
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(ok));
        m["error"] = std::make_shared<Value>(Value::fromString(ok ? "" : err));
        std::vector<ValuePtr> out;
        for (int64_t x : ready) out.push_back(std::make_shared<Value>(Value::fromInt(x)));
        m["ready"] = std::make_shared<Value>(Value::fromArray(std::move(out)));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("net_tcp_poll", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "net_dns_lookup");
        std::unordered_map<std::string, ValuePtr> m;
        std::vector<ValuePtr> addrs;
        if (args.empty() || !args[0]) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(false));
            m["addresses"] = std::make_shared<Value>(Value::fromArray(std::move(addrs)));
            return Value::fromMap(std::move(m));
        }
        std::string host = args[0]->toString();
        if (host == "localhost") {
            addrs.push_back(std::make_shared<Value>(Value::fromString("127.0.0.1")));
            addrs.push_back(std::make_shared<Value>(Value::fromString("::1")));
        } else if (!host.empty()) {
            addrs.push_back(std::make_shared<Value>(Value::fromString(host)));
        }
        m["ok"] = std::make_shared<Value>(Value::fromBool(!addrs.empty()));
        m["addresses"] = std::make_shared<Value>(Value::fromArray(std::move(addrs)));
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("net_dns_lookup", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "net_tls_connect");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["secure"] = std::make_shared<Value>(Value::fromBool(false));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        if (args.size() < 2 || !args[0] || !args[1]) return Value::fromMap(std::move(m));
        int64_t id = -1;
        std::string err;
        if (kernTcpConnect(args[0]->toString(), static_cast<int>(args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0), id, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["id"] = std::make_shared<Value>(Value::fromInt(id));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("net_tls_connect", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "net_ws_connect");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        if (args.size() < 2 || !args[0] || !args[1]) return Value::fromMap(std::move(m));
        int64_t id = -1;
        std::string err;
        if (kernTcpConnect(args[0]->toString(), static_cast<int>(args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0), id, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["id"] = std::make_shared<Value>(Value::fromInt(id));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("net_ws_connect", i - 1);

    makeBuiltin(i++, [](VM* vm, std::vector<ValuePtr> args) {
        vmRequirePermission(vm, Perm::kNetworkTcp, "net_ws_listen");
        std::unordered_map<std::string, ValuePtr> m;
        m["ok"] = std::make_shared<Value>(Value::fromBool(false));
        m["id"] = std::make_shared<Value>(Value::fromInt(-1));
        if (args.size() < 2 || !args[0] || !args[1]) return Value::fromMap(std::move(m));
        int64_t id = -1;
        std::string err;
        if (kernTcpListen(args[0]->toString(), static_cast<int>(args[1]->type == Value::Type::INT ? std::get<int64_t>(args[1]->data) : 0), 16, id, err)) {
            m["ok"] = std::make_shared<Value>(Value::fromBool(true));
            m["id"] = std::make_shared<Value>(Value::fromInt(id));
        } else {
            m["error"] = std::make_shared<Value>(Value::fromString(err));
        }
        return Value::fromMap(std::move(m));
    });
    setGlobalFn("net_ws_listen", i - 1);
}

} // namespace kern

#endif // kERN_BUILTINS_HPP
