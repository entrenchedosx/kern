/* *
 * kern Bytecode Encryption - ChaCha20 Stream Cipher
 *
 * Provides a lightweight, zero-dependency implementation of the ChaCha20
 * stream cipher for encrypting compiled bytecode at rest. This prevents
 * casual reverse-engineering and asset-ripping from .knb files.
 *
 * WHY ChaCha20 (not simple XOR, not AES):
 *   - Stream cipher: no padding needed, encrypt/decrypt are identical operations.
 *   - 20 rounds of diffusion: resistant to known-plaintext attacks (unlike
 *     single-layer XOR which leaks the key on known plaintext).
 *   - 256-bit key: brute-force infeasible.
 *   - Pure C++17: no external dependencies (no OpenSSL, no libsodium).
 *   - High performance: ~3-4 cycles/byte on modern CPUs.
 *
 * DEFENSE-IN-DEPTH NOTE:
 *   This is NOT military-grade DRM. A determined attacker with a debugger
 *   can extract the key from the binary and decrypt bytecode at runtime.
 *   The goal is to prevent casual dumping of compiled scripts, not to
 *   withstand a nation-state attack. For stronger protection, combine with:
 *   - Obfuscated key derivation (e.g., derive from binary checksum)
 *   - Anti-debug / integrity checks in the runtime
 *   - Tiered licensing with remote key servers for premium content
 */

#ifndef KERN_CRYPTO_HPP
#define KERN_CRYPTO_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <array>

namespace kern {
namespace crypto {

/* * ChaCha20 constants */
inline constexpr size_t kKeySize = 32;        // 256-bit key
inline constexpr size_t kNonceSize = 8;       // 64-bit nonce
inline constexpr size_t kBlockSize = 64;      // 512-bit block
inline constexpr size_t kRounds = 20;         // ChaCha20 rounds

/* * Default encryption key (compile-time constant).
 *  In production, this should be derived or overridden.
 *  Override via KERN_CRYPTO_KEY environment variable (hex-encoded, 64 hex chars).
 */
inline const std::array<uint8_t, kKeySize> kDefaultKey = {
    0x4B, 0x45, 0x52, 0x4E,  // "KERN"
    0x43, 0x52, 0x59, 0x50,  // "CRYP"
    0x54, 0x4F, 0x5F, 0x4B,  // "TO_K"
    0x45, 0x59, 0x5F, 0x32,  // "EY_2"
    0x30, 0x32, 0x36, 0x5F,  // "026_"
    0x56, 0x32, 0x2E, 0x31,  // "V2.1"
    0x2E, 0x30, 0x00, 0x00,  // ".0\0\0"
    0x00, 0x00, 0x00, 0x00   // padding
};

/* * Default nonce (derived from version).
 *  In production, each file should use a unique random nonce stored in the header.
 */
inline const std::array<uint8_t, kNonceSize> kDefaultNonce = {
    0x4B, 0x4E, 0x42, 0x46,  // "KNBF"
    0x56, 0x32, 0x00, 0x00   // "V2\0\0"
};

/* * Rotate left 32-bit integer */
inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

/* * ChaCha20 quarter round */
inline void quarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = rotl32(d, 16);
    c += d; b ^= c; b = rotl32(b, 12);
    a += b; d ^= a; d = rotl32(d, 8);
    c += d; b ^= c; b = rotl32(b, 7);
}

/* * Load 4 little-endian bytes into a uint32_t */
inline uint32_t load32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

/* * Store uint32_t as 4 little-endian bytes */
inline void store32le(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

/* * Initialize ChaCha20 state from key and nonce */
inline void chacha20InitState(uint32_t state[16], const uint8_t key[32], uint64_t counter, const uint8_t nonce[8]) {
    // Constants: "expand 32-byte k"
    state[0] = 0x61707865;  // "expa"
    state[1] = 0x3320646E;  // "nd 3"
    state[2] = 0x79622D32;  // "2-by"
    state[3] = 0x6B206574;  // "te k"

    // Key (8 words, 32 bytes)
    for (int i = 0; i < 8; i++)
        state[4 + i] = load32le(key + i * 4);

    // Counter (2 words, 64 bits)
    state[12] = static_cast<uint32_t>(counter & 0xFFFFFFFF);
    state[13] = static_cast<uint32_t>((counter >> 32) & 0xFFFFFFFF);

    // Nonce (2 words, 64 bits)
    state[14] = load32le(nonce);
    state[15] = load32le(nonce + 4);
}

/* * Generate one ChaCha20 block (64 bytes) at the given state (which has counter already set).
 *  Returns the keystream block and also mutates the working state (for the next block).
 */
inline void chacha20Block(const uint32_t state[16], uint8_t output[64]) {
    uint32_t working[16];
    memcpy(working, state, sizeof(uint32_t) * 16);

    // 20 rounds (10 double rounds)
    for (int i = 0; i < 10; i++) {
        // Column rounds
        quarterRound(working[0], working[4], working[8],  working[12]);
        quarterRound(working[1], working[5], working[9],  working[13]);
        quarterRound(working[2], working[6], working[10], working[14]);
        quarterRound(working[3], working[7], working[11], working[15]);
        // Diagonal rounds
        quarterRound(working[0], working[5], working[10], working[15]);
        quarterRound(working[1], working[6], working[11], working[12]);
        quarterRound(working[2], working[7], working[8],  working[13]);
        quarterRound(working[3], working[4], working[9],  working[14]);
    }

    // Add original state
    for (int i = 0; i < 16; i++) {
        store32le(output + i * 4, working[i] + state[i]);
    }
}

/* * Encrypt/decrypt data using ChaCha20 stream cipher.
 *  Since ChaCha20 is a stream cipher (XOR with keystream), encrypt and decrypt
 *  are the SAME operation.
 *
 *  @param data       Input data to encrypt/decrypt (mutated in-place)
 *  @param key        32-byte key
 *  @param nonce      8-byte nonce
 *  @param counter    Starting block counter (typically 0)
 */
inline void chacha20Xor(
    uint8_t* data,
    size_t len,
    const uint8_t key[32],
    const uint8_t nonce[8],
    uint64_t counter = 0
) {
    if (len == 0) return;

    uint32_t state[16];
    chacha20InitState(state, key, counter, nonce);

    uint8_t keystream[kBlockSize];
    size_t offset = 0;

    while (offset < len) {
        // Generate keystream block with current counter
        state[12] = static_cast<uint32_t>(counter & 0xFFFFFFFF);
        state[13] = static_cast<uint32_t>((counter >> 32) & 0xFFFFFFFF);

        chacha20Block(state, keystream);

        size_t chunk = (len - offset < kBlockSize) ? (len - offset) : kBlockSize;
        for (size_t i = 0; i < chunk; i++) {
            data[offset + i] ^= keystream[i];
        }

        offset += kBlockSize;
        counter++;
    }

    // Zero out keystream (defense against reuse)
    volatile uint8_t* vs = keystream;
    for (size_t i = 0; i < kBlockSize; i++) vs[i] = 0;
}

/* * High-level convenience: encrypt data with default key.
 *  @param data  Data to encrypt (mutated in-place)
 */
inline void encrypt(std::vector<uint8_t>& data) {
    chacha20Xor(data.data(), data.size(), kDefaultKey.data(), kDefaultNonce.data(), 0);
}

/* * High-level convenience: decrypt data with default key.
 *  @param data  Data to decrypt (mutated in-place)
 */
inline void decrypt(std::vector<uint8_t>& data) {
    // ChaCha20 is symmetric: encrypt and decrypt are the same operation
    chacha20Xor(data.data(), data.size(), kDefaultKey.data(), kDefaultNonce.data(), 0);
}

/* * Encrypt/decrypt a span of bytes with explicit key+nonce.
 *  @param data  Data to transform (mutated in-place)
 *  @param key   32-byte key
 *  @param nonce 8-byte nonce
 */
inline void chacha20(
    uint8_t* data,
    size_t len,
    const uint8_t key[32],
    const uint8_t nonce[8],
    uint64_t counter = 0
) {
    chacha20Xor(data, len, key, nonce, counter);
}

/* * Try to load a hex-encoded key from the environment.
 *  Returns true and fills |out| if KERN_CRYPTO_KEY is set and valid.
 */
inline bool tryLoadKeyFromEnv(std::array<uint8_t, kKeySize>& out) {
    const char* env = std::getenv("KERN_CRYPTO_KEY");
    if (!env || strlen(env) < kKeySize * 2) return false;

    // Parse hex string
    for (size_t i = 0; i < kKeySize; i++) {
        char hi = env[i * 2];
        char lo = env[i * 2 + 1];
        auto hexVal = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
            return 0;
        };
        out[i] = static_cast<uint8_t>((hexVal(hi) << 4) | hexVal(lo));
    }
    return true;
}

/* * Get the effective encryption key (environment override falls back to default). */
inline std::array<uint8_t, kKeySize> getEffectiveKey() {
    std::array<uint8_t, kKeySize> key;
    if (tryLoadKeyFromEnv(key))
        return key;
    return kDefaultKey;
}

} // namespace crypto
} // namespace kern

#endif // KERN_CRYPTO_HPP
