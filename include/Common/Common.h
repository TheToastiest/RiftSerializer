// RiftSerializer/include/RiftSerializer/Common.h
#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <algorithm>
#include <cstdlib>

#define SPDLOG_HEADER_ONLY
#include <spdlog/spdlog.h>

// --- Fixed-Width Integer Aliases ---
namespace RiftSerializer {
    using int8 = std::int8_t;
    using uint8 = std::uint8_t;
    using int16 = std::int16_t;
    using uint16 = std::uint16_t;
    using int32 = std::int32_t;
    using uint32 = std::uint32_t;
    using int64 = std::int64_t;
    using uint64 = std::uint64_t;
}

// --- Endianness Detection & Conversion ---
namespace RiftSerializer {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RIFT_SERIALIZER_HOST_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define RIFT_SERIALIZER_HOST_BIG_ENDIAN 1
#elif defined(_MSC_VER)
#define RIFT_SERIALIZER_HOST_LITTLE_ENDIAN 1
#else
#error "Cannot determine host endianness."
#endif

    namespace detail {
        inline uint16_t swap_bytes(uint16_t v) {
#ifdef _MSC_VER
            return _byteswap_ushort(v);
#else
            return __builtin_bswap16(v);
#endif
        }
        inline uint32_t swap_bytes(uint32_t v) {
#ifdef _MSC_VER
            return _byteswap_ulong(v);
#else
            return __builtin_bswap32(v);
#endif
        }
        inline uint64_t swap_bytes(uint64_t v) {
#ifdef _MSC_VER
            return _byteswap_uint64(v);
#else
            return __builtin_bswap64(v);
#endif
        }

        template<typename T>
        inline T to_little_endian(T value) {
#ifdef RIFT_SERIALIZER_HOST_BIG_ENDIAN
            if constexpr (sizeof(T) == 2) return static_cast<T>(swap_bytes(static_cast<uint16>(value)));
            if constexpr (sizeof(T) == 4) return static_cast<T>(swap_bytes(static_cast<uint32>(value)));
            if constexpr (sizeof(T) == 8) return static_cast<T>(swap_bytes(static_cast<uint64>(value)));
#endif
            return value;
        }

        template<typename T>
        inline T from_little_endian(T value) {
            return to_little_endian(value); // Same operation
        }
    } // namespace detail

    using detail::to_little_endian;
    using detail::from_little_endian;
}

// --- Alignment Utilities ---
namespace RiftSerializer {
    inline size_t align_up(size_t offset, size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }
    inline bool is_aligned(const void* ptr, size_t alignment) {
        return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
    }
}

// --- Assertion Macro ---
#ifdef RIFT_SERIALIZER_DEBUG
#define RIFT_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                spdlog::critical("ASSERT FAILED: {} ({}:{})", message, __FILE__, __LINE__); \
                std::abort(); \
            } \
        } while (0)
#else
#define RIFT_ASSERT(condition, message)
#endif