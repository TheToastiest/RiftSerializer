// RiftSerializer/include/RiftSerializer/Common.h
//
// Core utilities and definitions for the RiftSerializer library.
// This header provides fixed-width integer types, endianness conversion
// functions, alignment utilities, and basic assertion macros using spdlog.

#pragma once // Ensures this header is included only once per compilation unit.

#include <cstdint>     // For fixed-width integer types (uint32_t, etc.)
#include <cstring>     // For std::memcpy (used in float/double bit-casting)
#include <type_traits> // For std::enable_if, std::is_integral, etc.
#include <algorithm>   // For std::min, std::max
#include <cstdlib>     // For std::abort

// --- spdlog Integration for Assertions ---
// Ensure SPDLOG_HEADER_ONLY is defined in your project's build settings (e.g., CMake, VS Project Properties)
// for spdlog to operate in header-only mode.
// Also, initialize spdlog in your application's main function or engine setup.
#define SPDLOG_HEADER_ONLY // Required for spdlog to be header-only
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> // Provides basic console sink if no logger is set up

// --- Magic Number and Global Constants ---

namespace RiftSerializer {

    // A magic number to identify RiftSerializer buffers.
    // Using 'RFS0' as a simple example.
    // Stored in Little Endian in the buffer.
    constexpr uint32_t RIFT_MAGIC_NUMBER = 0x30534652; // 'RFS0' in Little Endian

} // namespace RiftSerializer

// --- Fixed-Width Integer Aliases ---
// Using C++11 'using' aliases for clarity and consistency.

namespace RiftSerializer {

    using int8 = std::int8_t;
    using uint8 = std::uint8_t;
    using int16 = std::int16_t;
    using uint16 = std::uint16_t;
    using int32 = std::int32_t;
    using uint32 = std::uint32_t;
    using int64 = std::int64_t;
    using uint64 = std::uint64_t;

    // Static assertions to ensure these types have the expected sizes.
    // This is crucial for cross-platform binary compatibility.
    static_assert(sizeof(int8) == 1, "int8_t is not 1 byte");
    static_assert(sizeof(uint8) == 1, "uint8_t is not 1 byte");
    static_assert(sizeof(int16) == 2, "int16_t is not 2 bytes");
    static_assert(sizeof(uint16) == 2, "uint16_t is not 2 bytes");
    static_assert(sizeof(int32) == 4, "int32_t is not 4 bytes");
    static_assert(sizeof(uint32) == 4, "uint32_t is not 4 bytes");
    static_assert(sizeof(int64) == 8, "int64_t is not 8 bytes");
    static_assert(sizeof(uint64) == 8, "uint64_t is not 8 bytes");
    static_assert(sizeof(float) == 4, "float is not 4 bytes (IEEE 754 single precision)");
    static_assert(sizeof(double) == 8, "double is not 8 bytes (IEEE 754 double precision)");

} // namespace RiftSerializer

// --- Endianness Detection ---
// Detects the host system's endianness at compile time.
// This is critical for knowing whether to perform byte-swapping.

namespace RiftSerializer {
    namespace detail {

        // Check if the host system is Little Endian.
        // Uses a simple compile-time trick.
        constexpr bool is_little_endian() {
            union {
                uint32_t i;
                uint8_t c[4];
            } u = { 1 }; // If little-endian, u.c[0] will be 1.
            return u.c[0] == 1;
        }

        // Define RIFT_SERIALIZER_HOST_LITTLE_ENDIAN and RIFT_SERIALIZER_HOST_BIG_ENDIAN
        // for conditional compilation.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    // GCC/Clang specific macro for explicit little endian
#define RIFT_SERIALIZER_HOST_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    // GCC/Clang specific macro for explicit big endian
#define RIFT_SERIALIZER_HOST_BIG_ENDIAN 1
#elif defined(_MSC_VER) // MSVC is typically Little Endian
#define RIFT_SERIALIZER_HOST_LITTLE_ENDIAN 1
#else
    // Fallback for other compilers/platforms. If you explicitly target
    // only Little Endian platforms, you can remove this.
#error "Cannot determine host endianness. Please define RIFT_SERIALIZER_HOST_LITTLE_ENDIAN or RIFT_SERIALIZER_HOST_BIG_ENDIAN."
#endif

    } // namespace detail
} // namespace RiftSerializer

// --- Byte Swapping Functions ---
// Provides functions to swap bytes for multi-byte types.
// These are optimized with compiler intrinsics where available.

namespace RiftSerializer {
    namespace detail {

        // Generic byte-swap function (might be slow, prefer intrinsics)
        template<typename T>
        T swap_bytes_generic(T value) {
            static_assert(std::is_integral<T>::value, "swap_bytes_generic only works with integral types.");
            T result = 0;
            for (size_t i = 0; i < sizeof(T); ++i) {
                result |= (static_cast<T>((value >> (i * 8)) & 0xFF) << ((sizeof(T) - 1 - i) * 8));
            }
            return result;
        }

        // Optimized byte-swap for 16-bit integers
        inline uint16_t swap_bytes(uint16_t value) {
#ifdef _MSC_VER
            return _byteswap_ushort(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap16(value);
#else
            return swap_bytes_generic(value);
#endif
        }

        // Optimized byte-swap for 32-bit integers
        inline uint32_t swap_bytes(uint32_t value) {
#ifdef _MSC_VER
            return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap32(value);
#else
            return swap_bytes_generic(value);
#endif
        }

        // Optimized byte-swap for 64-bit integers
        inline uint64_t swap_bytes(uint64_t value) {
#ifdef _MSC_VER
            return _byteswap_uint64(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap64(value);
#else
            return swap_bytes_generic(value);
#endif
        }

        // --- Endian Conversion Functions ---
        // These functions convert values to/from the canonical Little Endian format.
        // They are no-ops on Little Endian hosts, and perform swaps on Big Endian hosts.

        template<typename T, std::enable_if_t<std::is_integral<T>::value && !std::is_same_v<T, bool>, int> = 0>
        inline T to_little_endian(T value) {
#ifdef RIFT_SERIALIZER_HOST_BIG_ENDIAN
            return static_cast<T>(swap_bytes(static_cast<std::make_unsigned_t<T>>(value)));
#else
            return value; // No swap needed for Little Endian host
#endif
        }

        template<typename T, std::enable_if_t<std::is_integral<T>::value && !std::is_same_v<T, bool>, int> = 0>
        inline T from_little_endian(T value) {
#ifdef RIFT_SERIALIZER_HOST_BIG_ENDIAN
            return static_cast<T>(swap_bytes(static_cast<std::make_unsigned_t<T>>(value)));
#else
            return value; // No swap needed for Little Endian host
#endif
        }

        // Floating point endian conversion: requires bit-casting to integer, then swapping.
        inline float to_little_endian(float value) {
            uint32_t u32_val;
            static_assert(sizeof(float) == sizeof(uint32_t), "Float and uint32_t size mismatch for bit-casting.");
            std::memcpy(&u32_val, &value, sizeof(float));
            u32_val = to_little_endian(u32_val);
            float result;
            std::memcpy(&result, &u32_val, sizeof(float));
            return result;
        }

        inline float from_little_endian(float value) {
            uint32_t u32_val;
            static_assert(sizeof(float) == sizeof(uint32_t), "Float and uint32_t size mismatch for bit-casting.");
            std::memcpy(&u32_val, &value, sizeof(float));
            u32_val = from_little_endian(u32_val);
            float result;
            std::memcpy(&result, &u32_val, sizeof(float));
            return result;
        }

        inline double to_little_endian(double value) {
            uint64_t u64_val;
            static_assert(sizeof(double) == sizeof(uint64_t), "Double and uint64_t size mismatch for bit-casting.");
            std::memcpy(&u64_val, &value, sizeof(double));
            u64_val = to_little_endian(u64_val);
            double result;
            std::memcpy(&result, &u64_val, sizeof(double));
            return result;
        }

        inline double from_little_endian(double value) {
            uint64_t u64_val;
            static_assert(sizeof(double) == sizeof(uint64_t), "Double and uint64_t size mismatch for bit-casting.");
            std::memcpy(&u64_val, &value, sizeof(double));
            u64_val = from_little_endian(u64_val);
            double result;
            std::memcpy(&result, &u64_val, sizeof(double));
            return result;
        }

        // Handle bool specifically: serialize as 1-byte uint8
        inline uint8 to_little_endian(bool value) { return static_cast<uint8>(value); }
        inline bool from_little_endian(uint8 value) { return static_cast<bool>(value); } // bool deserialization from uint8


    } // namespace detail

    // Public aliases for endian conversion functions
    using detail::to_little_endian;
    using detail::from_little_endian;

} // namespace RiftSerializer


// --- Alignment Utilities ---
// Functions for calculating and checking memory alignment.

namespace RiftSerializer {

    // Aligns 'offset' up to the next multiple of 'alignment'.
    // 'alignment' must be a power of 2.
    inline size_t align_up(size_t offset, size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    // Checks if 'value' is aligned to 'alignment'.
    // 'alignment' must be a power of 2.
    inline bool is_aligned(size_t value, size_t alignment) {
        return (value % alignment) == 0;
    }

    // Get the natural alignment of a type
    template<typename T>
    inline constexpr size_t get_natural_alignment() {
        return alignof(T);
    }

} // namespace RiftSerializer


// --- Assertion and Debugging Macros ---
// Simple assertion macro for internal debug checks using spdlog.
// Ensure SPDLOG_HEADER_ONLY is defined in your project's build settings.
// spdlog requires initialization in your application's main function or engine setup.

#ifdef RIFT_SERIALIZER_DEBUG
#define RIFT_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                /* Use spdlog::critical for assertions that indicate unrecoverable errors */ \
                spdlog::critical("RIFT_ASSERTION_FAILED: {} ({}:{})", message, __FILE__, __LINE__); \
                std::abort(); /* Terminate the program */ \
            } \
        } while (0)
#else
    // In release builds, assertions compile to nothing.
#define RIFT_ASSERT(condition, message) do { (void)(condition); } while (0)
#endif
