/* *
 * kern Bytecode Serializer - Binary format with optional encryption
 *
 * Provides serialization and deserialization of compiled bytecode
 * (Bytecode + string constants + value constants) to/from disk.
 *
 * File format (.knb):
 *   [BytecodeHeader]  16 bytes  (magic, version, flags, reserved)
 *   [Encrypted Payload]  variable  (ChaCha20 encrypted)
 *     ├── instruction_count  uint32_t
 *     ├── instructions[]     variable  (see serializeInstruction)
 *     ├── string_count       uint32_t
 *     ├── strings[]          variable  (length-prefixed UTF-8)
 *     ├── value_count        uint32_t
 *     └── values[]           variable  (type-tagged)
 *
 * When BytecodeFlags::ENCRYPTED is set in the header flags,
 * the payload is encrypted with ChaCha20. Without the flag,
 * the payload is stored as plaintext (for debugging/tooling).
 */

#ifndef KERN_BYTECODE_SERIALIZER_HPP
#define KERN_BYTECODE_SERIALIZER_HPP

#include "bytecode/bytecode.hpp"
#include "bytecode/bytecode_header.hpp"
#include "bytecode/value.hpp"
#include "bytecode/crypto.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace kern {

/* * Custom exception for bytecode serialization errors */
class BytecodeSerializeError : public std::runtime_error {
public:
    explicit BytecodeSerializeError(const std::string& msg)
        : std::runtime_error(msg) {}
};

/* * Extended flags for encryption support */
enum class BytecodeSerializerFlags : uint16_t {
    NONE = 0,
    ENCRYPTED = 1 << 4   // Payload is ChaCha20 encrypted
};

/* *
 * Serialize a single instruction to a byte vector.
 * Format:
 *   [opcode]      1 byte   (uint8_t)
 *   [line]        4 bytes  (int32_t, little-endian)
 *   [column]      4 bytes  (int32_t, little-endian)
 *   [operand_type] 1 byte  (0=monostate, 1=int64, 2=double, 3=string, 4=size_t, 5=pair)
 *   [operand_data] variable
 */
inline void serializeInstruction(std::vector<uint8_t>& out, const Instruction& inst) {
    // Opcode (1 byte)
    out.push_back(static_cast<uint8_t>(inst.op));

    // Line (4 bytes, little-endian)
    int32_t line = static_cast<int32_t>(inst.line);
    out.push_back(static_cast<uint8_t>(line & 0xFF));
    out.push_back(static_cast<uint8_t>((line >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((line >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((line >> 24) & 0xFF));

    // Column (4 bytes, little-endian)
    int32_t col = static_cast<int32_t>(inst.column);
    out.push_back(static_cast<uint8_t>(col & 0xFF));
    out.push_back(static_cast<uint8_t>((col >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((col >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((col >> 24) & 0xFF));

    // Operand type (1 byte)
    uint8_t opType = static_cast<uint8_t>(inst.operand.index());
    out.push_back(opType);

    // Operand data (variable)
    switch (inst.operand.index()) {
        case 0: // monostate - no data
            break;
        case 1: { // int64_t (8 bytes)
            int64_t v = std::get<int64_t>(inst.operand);
            for (int i = 0; i < 8; i++) {
                out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
            }
            break;
        }
        case 2: { // double (8 bytes)
            double v = std::get<double>(inst.operand);
            uint64_t raw;
            memcpy(&raw, &v, sizeof(raw));
            for (int i = 0; i < 8; i++) {
                out.push_back(static_cast<uint8_t>((raw >> (i * 8)) & 0xFF));
            }
            break;
        }
        case 3: { // string: length (4 bytes) + data
            const std::string& s = std::get<std::string>(inst.operand);
            uint32_t len = static_cast<uint32_t>(s.size());
            for (int i = 0; i < 4; i++)
                out.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
            for (char c : s)
                out.push_back(static_cast<uint8_t>(c));
            break;
        }
        case 4: { // size_t (8 bytes)
            uint64_t v = static_cast<uint64_t>(std::get<size_t>(inst.operand));
            for (int i = 0; i < 8; i++) {
                out.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
            }
            break;
        }
        case 5: { // pair<size_t,size_t>: a (8 bytes) + b (8 bytes)
            const auto& p = std::get<std::pair<size_t, size_t>>(inst.operand);
            uint64_t a = static_cast<uint64_t>(p.first);
            uint64_t b = static_cast<uint64_t>(p.second);
            for (int i = 0; i < 8; i++)
                out.push_back(static_cast<uint8_t>((a >> (i * 8)) & 0xFF));
            for (int i = 0; i < 8; i++)
                out.push_back(static_cast<uint8_t>((b >> (i * 8)) & 0xFF));
            break;
        }
    }
}

/* * Deserialize a single instruction from a byte buffer.
 *  Returns the number of bytes consumed.
 */
inline size_t deserializeInstruction(const uint8_t* data, size_t len, Instruction& out) {
    if (len < 10)  // opcode(1) + line(4) + column(4) + operType(1) = 10 minimum
        throw BytecodeSerializeError("Unexpected end of data while deserializing instruction");

    size_t pos = 0;

    // Opcode
    uint8_t opVal = data[pos++];
    out.op = static_cast<Opcode>(opVal);

    // Line
    int32_t line = static_cast<int32_t>(data[pos]) |
                   (static_cast<int32_t>(data[pos + 1]) << 8) |
                   (static_cast<int32_t>(data[pos + 2]) << 16) |
                   (static_cast<int32_t>(data[pos + 3]) << 24);
    out.line = static_cast<int>(line);
    pos += 4;

    // Column
    int32_t col = static_cast<int32_t>(data[pos]) |
                  (static_cast<int32_t>(data[pos + 1]) << 8) |
                  (static_cast<int32_t>(data[pos + 2]) << 16) |
                  (static_cast<int32_t>(data[pos + 3]) << 24);
    out.column = static_cast<int>(col);
    pos += 4;

    // Operand type
    uint8_t opType = data[pos++];

    switch (opType) {
        case 0: // monostate
            out.operand = std::monostate{};
            break;
        case 1: { // int64_t
            if (pos + 8 > len) throw BytecodeSerializeError("Unexpected end of data for int64 operand");
            int64_t v = 0;
            for (int i = 0; i < 8; i++)
                v |= static_cast<int64_t>(data[pos++]) << (i * 8);
            out.operand = v;
            break;
        }
        case 2: { // double
            if (pos + 8 > len) throw BytecodeSerializeError("Unexpected end of data for double operand");
            uint64_t raw = 0;
            for (int i = 0; i < 8; i++)
                raw |= static_cast<uint64_t>(data[pos++]) << (i * 8);
            double v;
            memcpy(&v, &raw, sizeof(v));
            out.operand = v;
            break;
        }
        case 3: { // string
            if (pos + 4 > len) throw BytecodeSerializeError("Unexpected end of data for string length");
            uint32_t slen = static_cast<uint32_t>(data[pos]) |
                           (static_cast<uint32_t>(data[pos + 1]) << 8) |
                           (static_cast<uint32_t>(data[pos + 2]) << 16) |
                           (static_cast<uint32_t>(data[pos + 3]) << 24);
            pos += 4;
            if (pos + slen > len) throw BytecodeSerializeError("Unexpected end of data for string content");
            std::string s(reinterpret_cast<const char*>(data + pos), slen);
            pos += slen;
            out.operand = std::move(s);
            break;
        }
        case 4: { // size_t
            if (pos + 8 > len) throw BytecodeSerializeError("Unexpected end of data for size_t operand");
            uint64_t v = 0;
            for (int i = 0; i < 8; i++)
                v |= static_cast<uint64_t>(data[pos++]) << (i * 8);
            out.operand = static_cast<size_t>(v);
            break;
        }
        case 5: { // pair<size_t,size_t>
            if (pos + 16 > len) throw BytecodeSerializeError("Unexpected end of data for pair operand");
            uint64_t a = 0, b = 0;
            for (int i = 0; i < 8; i++)
                a |= static_cast<uint64_t>(data[pos++]) << (i * 8);
            for (int i = 0; i < 8; i++)
                b |= static_cast<uint64_t>(data[pos++]) << (i * 8);
            out.operand = std::pair<size_t, size_t>(static_cast<size_t>(a), static_cast<size_t>(b));
            break;
        }
        default:
            throw BytecodeSerializeError(std::string("Unknown operand type in serialized bytecode: opType=") + std::to_string(static_cast<int>(opType)));
    }

    return pos;
}

/* * Serialize a Value to a byte vector.
 *  Format: [type] 1 byte + [data] variable
 */
inline void serializeValue(std::vector<uint8_t>& out, const Value& v) {
    out.push_back(static_cast<uint8_t>(v.type));

    switch (v.type) {
        case Value::Type::NIL:
            break;
        case Value::Type::BOOL: {
            uint8_t b = std::get<bool>(v.data) ? 1 : 0;
            out.push_back(b);
            break;
        }
        case Value::Type::INT: {
            int64_t val = std::get<int64_t>(v.data);
            for (int i = 0; i < 8; i++)
                out.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
            break;
        }
        case Value::Type::FLOAT: {
            double val = std::get<double>(v.data);
            uint64_t raw;
            memcpy(&raw, &val, sizeof(raw));
            for (int i = 0; i < 8; i++)
                out.push_back(static_cast<uint8_t>((raw >> (i * 8)) & 0xFF));
            break;
        }
        case Value::Type::STRING: {
            const std::string& s = std::get<std::string>(v.data);
            uint32_t len = static_cast<uint32_t>(s.size());
            for (int i = 0; i < 4; i++)
                out.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
            for (char c : s)
                out.push_back(static_cast<uint8_t>(c));
            break;
        }
        default:
            // For ARRAY, MAP, FUNCTION, etc. — not serialized in valueConstants
            // Throw or store placeholder
            throw BytecodeSerializeError("Unsupported Value type in constant pool: " + v.typeName());
    }
}

/* * Deserialize a Value from a byte buffer.
 *  Returns the number of bytes consumed.
 */
inline size_t deserializeValue(const uint8_t* data, size_t len, Value& out) {
    if (len < 1) throw BytecodeSerializeError("Unexpected end of data for value type");

    size_t pos = 0;
    uint8_t typeVal = data[pos++];
    out.type = static_cast<Value::Type>(typeVal);

    switch (out.type) {
        case Value::Type::NIL:
            out.data = std::monostate{};
            break;
        case Value::Type::BOOL: {
            if (pos + 1 > len) throw BytecodeSerializeError("Unexpected end of data for bool value");
            out.data = (data[pos++] != 0);
            break;
        }
        case Value::Type::INT: {
            if (pos + 8 > len) throw BytecodeSerializeError("Unexpected end of data for int value");
            int64_t val = 0;
            for (int i = 0; i < 8; i++)
                val |= static_cast<int64_t>(data[pos++]) << (i * 8);
            out.data = val;
            break;
        }
        case Value::Type::FLOAT: {
            if (pos + 8 > len) throw BytecodeSerializeError("Unexpected end of data for float value");
            uint64_t raw = 0;
            for (int i = 0; i < 8; i++)
                raw |= static_cast<uint64_t>(data[pos++]) << (i * 8);
            double val;
            memcpy(&val, &raw, sizeof(val));
            out.data = val;
            break;
        }
        case Value::Type::STRING: {
            if (pos + 4 > len) throw BytecodeSerializeError("Unexpected end of data for string length");
            uint32_t slen = static_cast<uint32_t>(data[pos]) |
                           (static_cast<uint32_t>(data[pos + 1]) << 8) |
                           (static_cast<uint32_t>(data[pos + 2]) << 16) |
                           (static_cast<uint32_t>(data[pos + 3]) << 24);
            pos += 4;
            if (pos + slen > len) throw BytecodeSerializeError("Unexpected end of data for string content");
            std::string s(reinterpret_cast<const char*>(data + pos), slen);
            pos += slen;
            out.data = std::move(s);
            break;
        }
        default:
            throw BytecodeSerializeError("Unsupported Value type in constant pool: " + std::to_string(typeVal));
    }

    return pos;
}

/* *
 * Serialize full bytecode (instructions + constants) to a byte vector.
 * Does NOT include the header — this is the raw payload.
 */
inline std::vector<uint8_t> serializePayload(
    const Bytecode& code,
    const std::vector<std::string>& stringConstants,
    const std::vector<Value>& valueConstants
) {
    std::vector<uint8_t> out;

    // Instruction count
    uint32_t instCount = static_cast<uint32_t>(code.size());
    for (int i = 0; i < 4; i++)
        out.push_back(static_cast<uint8_t>((instCount >> (i * 8)) & 0xFF));

    // Instructions
    for (const auto& inst : code) {
        serializeInstruction(out, inst);
    }

    // String constant count
    uint32_t strCount = static_cast<uint32_t>(stringConstants.size());
    for (int i = 0; i < 4; i++)
        out.push_back(static_cast<uint8_t>((strCount >> (i * 8)) & 0xFF));

    // String constants: each is [length:4bytes][data]
    for (const auto& s : stringConstants) {
        uint32_t len = static_cast<uint32_t>(s.size());
        for (int i = 0; i < 4; i++)
            out.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        for (char c : s)
            out.push_back(static_cast<uint8_t>(c));
    }

    // Value constant count
    uint32_t valCount = static_cast<uint32_t>(valueConstants.size());
    for (int i = 0; i < 4; i++)
        out.push_back(static_cast<uint8_t>((valCount >> (i * 8)) & 0xFF));

    // Value constants
    for (const auto& v : valueConstants) {
        serializeValue(out, v);
    }

    return out;
}

/* *
 * Deserialize payload from a byte buffer.
 * Returns the deserialized bytecode, string constants, and value constants.
 */
inline void deserializePayload(
    const uint8_t* data,
    size_t len,
    Bytecode& outCode,
    std::vector<std::string>& outStrings,
    std::vector<Value>& outValues
) {
    size_t pos = 0;

    // Instruction count
    if (pos + 4 > len) throw BytecodeSerializeError("Unexpected end of data for instruction count");
    uint32_t instCount = static_cast<uint32_t>(data[pos]) |
                        (static_cast<uint32_t>(data[pos + 1]) << 8) |
                        (static_cast<uint32_t>(data[pos + 2]) << 16) |
                        (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;

    // Instructions
    outCode.clear();
    outCode.reserve(instCount);
    for (uint32_t i = 0; i < instCount; i++) {
        Instruction inst(Opcode::NOP);
        size_t consumed = deserializeInstruction(data + pos, len - pos, inst);
        pos += consumed;
        outCode.push_back(std::move(inst));
    }

    // String constant count
    if (pos + 4 > len) throw BytecodeSerializeError("Unexpected end of data for string count");
    uint32_t strCount = static_cast<uint32_t>(data[pos]) |
                       (static_cast<uint32_t>(data[pos + 1]) << 8) |
                       (static_cast<uint32_t>(data[pos + 2]) << 16) |
                       (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;

    // String constants
    outStrings.clear();
    outStrings.reserve(strCount);
    for (uint32_t i = 0; i < strCount; i++) {
        if (pos + 4 > len) throw BytecodeSerializeError("Unexpected end of data for string length");
        uint32_t slen = static_cast<uint32_t>(data[pos]) |
                       (static_cast<uint32_t>(data[pos + 1]) << 8) |
                       (static_cast<uint32_t>(data[pos + 2]) << 16) |
                       (static_cast<uint32_t>(data[pos + 3]) << 24);
        pos += 4;
        if (pos + slen > len) throw BytecodeSerializeError("Unexpected end of data for string content");
        outStrings.emplace_back(reinterpret_cast<const char*>(data + pos), slen);
        pos += slen;
    }

    // Value constant count
    if (pos + 4 > len) throw BytecodeSerializeError("Unexpected end of data for value count");
    uint32_t valCount = static_cast<uint32_t>(data[pos]) |
                       (static_cast<uint32_t>(data[pos + 1]) << 8) |
                       (static_cast<uint32_t>(data[pos + 2]) << 16) |
                       (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;

    // Value constants
    outValues.clear();
    outValues.reserve(valCount);
    for (uint32_t i = 0; i < valCount; i++) {
        Value v;
        size_t consumed = deserializeValue(data + pos, len - pos, v);
        pos += consumed;
        outValues.push_back(std::move(v));
    }
}

/* *
 * Save bytecode to a file in .knb format with optional encryption.
 *
 * @param path            Output file path
 * @param code            The bytecode to save
 * @param stringConstants String constant pool
 * @param valueConstants  Value constant pool
 * @param encrypt         If true, encrypt the payload with ChaCha20
 * @param sourcePath      Optional source path for diagnostics
 * @return true on success, false on failure
 */
inline bool saveBytecodeToFile(
    const std::string& path,
    const Bytecode& code,
    const std::vector<std::string>& stringConstants,
    const std::vector<Value>& valueConstants,
    bool encrypt = true,
    const std::string& sourcePath = ""
) {
    // Serialize payload first so we know its size
    std::vector<uint8_t> payload = serializePayload(code, stringConstants, valueConstants);

    // Build header with payload size
    BytecodeHeader header;
    header.payloadSize = static_cast<uint32_t>(payload.size());
    if (encrypt) {
        header.flags |= static_cast<uint16_t>(BytecodeSerializerFlags::ENCRYPTED);
    }

    // Serialize header bytes
    std::vector<uint8_t> headerBytes = serializeHeader(header);

    // Optionally encrypt payload
    if (encrypt && !payload.empty()) {
        crypto::encrypt(payload);
    }

    // Write file: header + payload
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    f.write(reinterpret_cast<const char*>(headerBytes.data()), headerBytes.size());
    f.write(reinterpret_cast<const char*>(payload.data()), payload.size());

    return static_cast<bool>(f);
}

/* *
 * Load bytecode from a .knb file with automatic decryption.
 *
 * @param path            Input file path
 * @param outCode         [out] The deserialized bytecode
 * @param outStrings      [out] String constant pool
 * @param outValues       [out] Value constant pool
 * @param sourcePath      [out] Optional source path for diagnostics
 * @return true on success, false on failure
 */
inline bool loadBytecodeFromFile(
    const std::string& path,
    Bytecode& outCode,
    std::vector<std::string>& outStrings,
    std::vector<Value>& outValues,
    std::string* sourcePath = nullptr
) {
    // Read entire file
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;

    std::streamsize size = f.tellg();
    if (size < 16) return false;  // Too small for valid header

    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!f.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return false;
    }
    f.close();

    // Deserialize header
    BytecodeHeader header = deserializeHeader(buffer.data(), buffer.size());
    if (!header.isValid()) {
        return false;
    }

    // Check if encrypted
    bool isEncrypted = (header.flags & static_cast<uint16_t>(BytecodeSerializerFlags::ENCRYPTED)) != 0;

    // The payload starts after the 16-byte header
    constexpr size_t kHeaderSize = 16;
    if (kHeaderSize >= buffer.size()) return false;

    std::vector<uint8_t> payload(buffer.begin() + static_cast<ptrdiff_t>(kHeaderSize), buffer.end());

    // Decrypt if encrypted
    if (isEncrypted && !payload.empty()) {
        crypto::decrypt(payload);
    }

    // Deserialize payload
    deserializePayload(payload.data(), payload.size(), outCode, outStrings, outValues);

    return true;
}

} // namespace kern

#endif // KERN_BYTECODE_SERIALIZER_HPP
