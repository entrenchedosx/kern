/* *
 * kern Standard Library - Builtin functions registered with the VM
 */ 

#ifndef KERN_BUILTINS_HPP
#define KERN_BUILTINS_HPP

#include "vm.hpp"
#include "errors/errors.hpp"         // For VMError
#include "bytecode/value.hpp"
#include "bytecode/script_code.hpp"
#include "permissions.hpp"           // Or wherever RuntimeGuardPolicy is defined
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
        if (get() != '[') return std::make_shared<Value>(Value::fromArray(ValueArray{}));
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
        if (get() != '{') return std::make_shared<Value>(Value::fromMap(ValueMap{}));
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

namespace pool {
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
static_assert(!pool::transition_allowed[static_cast<int>(PoolSlotState::INVALID)][static_cast<int>(PoolSlotState::FREE)],
              "INVALID → FREE transition must be forbidden - INVALID is terminal");
static_assert(!pool::transition_allowed[static_cast<int>(PoolSlotState::INVALID)][static_cast<int>(PoolSlotState::ALLOCATED)],
              "INVALID → ALLOCATED transition must be forbidden - INVALID is terminal");
static_assert(!pool::transition_allowed[static_cast<int>(PoolSlotState::FREE)][static_cast<int>(PoolSlotState::INVALID)],
              "FREE → INVALID transition must be forbidden - only ANY → INVALID via destroyPool");
static_assert(pool::transition_allowed[static_cast<int>(PoolSlotState::FREE)][static_cast<int>(PoolSlotState::ALLOCATED)],
              "FREE → ALLOCATED transition must be allowed - this is the allocation path");
static_assert(pool::transition_allowed[static_cast<int>(PoolSlotState::ALLOCATED)][static_cast<int>(PoolSlotState::FREE)],
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
            if (!safe::safe_mul(blk.count, blockSize, total)) return false;
            
            char* start = blk.base;
            char* end = safe::safe_range_end(blk.base, blk.count, blockSize);
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
    if (!safe::safe_mul(blockSize, count, totalSize)) {
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
        if (!safe::safe_add(totalSlots, blk.count, totalSlots)) {
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
    if (!pool::transition_state(currentState, PoolSlotState::ALLOCATED, blockIdx, slotIdx)) {
        // transition_state handles debug abort internally; here we just return
        return nullptr;
    }
    
    // Execute the transition
    ps.setSlotState(blockIdx, slotIdx, PoolSlotState::ALLOCATED);
    
    // Compute pointer from block index and slot index using safe arithmetic
    const auto& blk = ps.blocks[blockIdx];
    char* ptr = safe::safe_index(blk.base, slotIdx, ps.blockSize);
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
    if (!pool::transition_state(currentState, PoolSlotState::FREE, blockIdx, slotIdx)) {
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

inline void MemoryManager::shutdown() {
    cleanupGlobalMemoryState();
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
    auto& m = std::get<ValueMap>(e->data);
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
        const auto& arr = std::get<ValueArray>(itt->second->data);
        for (const auto& fr : arr) {
            if (!fr || fr->type != Value::Type::MAP) continue;
            auto& fm = std::get<ValueMap>(fr->data);
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

void registerAllBuiltins(VM& vm);
} // namespace kern

#endif // KERN_BUILTINS_HPP

