/* *
 * Bytecode Header Format - Stable binary format for KERN releases
 * 
 * Provides version checking and forward compatibility.
 */

#ifndef KERN_BYTECODE_HEADER_HPP
#define KERN_BYTECODE_HEADER_HPP

#include <cstdint>
#include <cstddef>
#include <vector>
#include <stdexcept>

namespace kern {

/* * Current bytecode format version. Increment on breaking changes. */
inline constexpr uint16_t kBytecodeSchemaVersion = 2;  // Major version
inline constexpr uint8_t kBytecodeMinorVersion = 0;
inline constexpr uint8_t kBytecodePatchVersion = 3;
// Full version: 2.0.3

/* * Magic number "KERN" (big-endian: 0x4B45524E) */
inline constexpr uint32_t kBytecodeMagic = 0x4B45524E;

/* * Bytecode file header - 16 bytes fixed size */
struct BytecodeHeader {
    uint32_t magic;      // 0x4B45524E = "KERN"
    uint16_t version;    // Format version
    uint16_t flags;      // Feature flags
    uint32_t reserved;   // Reserved for future use
    
    /* * Default constructor initializes to valid defaults */
    BytecodeHeader() 
        : magic(kBytecodeMagic)
        , version(kBytecodeSchemaVersion)
        , flags(0)
        , reserved(0) {}
    
    /* * Validate header before use */
    bool isValid() const {
        return magic == kBytecodeMagic && version <= kBytecodeSchemaVersion;
    }
    
    /* * Check if version is compatible with current runtime */
    bool isCompatible() const {
        return magic == kBytecodeMagic && version == kBytecodeSchemaVersion;
    }
};

/* * Feature flags for bytecode capabilities */
enum class BytecodeFlags : uint16_t {
    NONE = 0,
    HAS_EXCEPTIONS = 1 << 0,      // Uses TRY/CATCH/THROW
    HAS_GENERATORS = 1 << 1,       // Uses YIELD
    HAS_FFI = 1 << 2,              // Uses foreign function interface
    HAS_GRAPHICS = 1 << 3,         // Uses g2d/g3d
    SAFE_MODE = 1 << 15            // Compiled with safety checks
};

/* * Exception thrown on invalid bytecode */
class BytecodeVersionError : public std::runtime_error {
public:
    BytecodeVersionError(const std::string& msg) 
        : std::runtime_error(msg) {}
};

/* * Serialize header to byte vector (little-endian) */
inline std::vector<uint8_t> serializeHeader(const BytecodeHeader& header) {
    std::vector<uint8_t> result;
    result.reserve(16);
    
    // Magic (4 bytes, big-endian for readability)
    result.push_back((header.magic >> 24) & 0xFF);
    result.push_back((header.magic >> 16) & 0xFF);
    result.push_back((header.magic >> 8) & 0xFF);
    result.push_back(header.magic & 0xFF);
    
    // Version (2 bytes, little-endian)
    result.push_back(header.version & 0xFF);
    result.push_back((header.version >> 8) & 0xFF);
    
    // Flags (2 bytes, little-endian)
    result.push_back(header.flags & 0xFF);
    result.push_back((header.flags >> 8) & 0xFF);
    
    // Reserved (4 bytes)
    result.push_back(0);
    result.push_back(0);
    result.push_back(0);
    result.push_back(0);
    
    return result;
}

/* * Deserialize header from bytes */
inline BytecodeHeader deserializeHeader(const uint8_t* data, size_t len) {
    if (len < 16) {
        throw BytecodeVersionError("Bytecode header too short (minimum 16 bytes required)");
    }
    
    BytecodeHeader header;
    
    // Magic (big-endian)
    header.magic = (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);
    
    // Version (little-endian)
    header.version = static_cast<uint16_t>(data[4]) |
                    (static_cast<uint16_t>(data[5]) << 8);
    
    // Flags (little-endian)
    header.flags = static_cast<uint16_t>(data[6]) |
                  (static_cast<uint16_t>(data[7]) << 8);
    
    // Reserved
    header.reserved = 0;
    
    return header;
}

} // namespace kern

#endif // KERN_BYTECODE_HEADER_HPP
