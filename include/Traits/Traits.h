// RiftSerializer/include/RiftSerializer/Traits.h
//
// Defines C++ type traits used by RiftSerializer to categorize types for
// optimized serialization and deserialization. This allows the system to
// distinguish between Plain Old Data (POD) types that can be directly memcpy'd
// and types that require special handling (e.g., endian conversion, variable sizing).

#pragma once // Ensures this header is included only once per compilation unit.

#include "../Common/Common.h"    // For fixed-width types and RIFT_ASSERT
#include <type_traits> // For std::is_standard_layout, std::is_trivially_copyable, etc.
#include <vector>      // For std::vector (example for variable size)
#include <string>      // For std::string (example for variable size)

namespace RiftSerializer {

    // --- 1. Rift POD (Plain Old Data) Trait ---
    // A type is considered a Rift POD if its binary representation can be directly
    // copied to and from a serialized buffer without any byte-swapping (assuming
    // it's composed of types that already handle their own endianness, or are
    // single-byte), and without any special construction/destruction.
    //
    // This is primarily for types that are naturally aligned and don't contain
    // pointers, virtual functions, or complex constructors/destructors.
    // For standard types (like `int`, `float`), we rely on standard type traits.
    // For custom structs (like Vec3, Quaternion), they need to be declared via
    // RIFT_SERIALIZER_DECLARE_RIFT_POD if they meet the criteria.

    namespace detail {
        // Default: assume not a Rift POD unless explicitly specialized or detected
        template<typename T>
        struct is_rift_pod_impl : std::false_type {};

        // Standard integral and floating-point types are inherently Rift PODs
        template<> struct is_rift_pod_impl<int8> : std::true_type {};
        template<> struct is_rift_pod_impl<uint8> : std::true_type {};
        template<> struct is_rift_pod_impl<int16> : std::true_type {};
        template<> struct is_rift_pod_impl<uint16> : std::true_type {};
        template<> struct is_rift_pod_impl<int32> : std::true_type {};
        template<> struct is_rift_pod_impl<uint32> : std::true_type {};
        template<> struct is_rift_pod_impl<int64> : std::true_type {};
        template<> struct is_rift_pod_impl<uint64> : std::true_type {};
        template<> struct is_rift_pod_impl<float> : std::true_type {};
        template<> struct is_rift_pod_impl<double> : std::true_type {};
        template<> struct is_rift_pod_impl<bool> : std::true_type {}; // bool is treated as uint8
    } // namespace detail

    template<typename T>
    struct is_rift_pod : detail::is_rift_pod_impl<T> {};

    // Macro to declare custom types as Rift PODs.
    // Use this for your `Vec3`, `Quaternion`, etc., if they truly are POD-like.
    // Requires that T be trivially copyable and standard layout.
#define RIFT_SERIALIZER_DECLARE_RIFT_POD(Type) \
    namespace RiftSerializer { \
    namespace detail { \
        template<> struct is_rift_pod_impl<Type> : std::true_type {}; \
    } /* namespace detail */ \
    } /* namespace RiftSerializer */ \
    static_assert(std::is_standard_layout<Type>::value, \
        #Type " declared as Rift POD but is not standard layout. Rift PODs must be standard layout."); \
    static_assert(std::is_trivially_copyable<Type>::value, \
        #Type " declared as Rift POD but is not trivially copyable. Rift PODs must be trivially copyable.");

// --- 2. Rift Fixed-Size Trait ---
// A type is fixed-size if its size in the serialized buffer is known at compile time.
// This is true for all Rift PODs, and potentially for other types that might
// have complex constructors but still occupy a fixed number of bytes.
// (e.g., a fixed-size array of non-PODs, if we had a policy for that).

// By default, if it's a Rift POD, it's fixed-size.
    template<typename T>
    struct is_rift_fixed_size : is_rift_pod<T> {};

    // Macro to declare custom types as fixed-size (even if not POD in the strictest sense).
    // Use this cautiously. If a type is fixed-size but not POD, it typically means
    // it needs custom serialization/deserialization logic, not just a memcpy.
#define RIFT_SERIALIZER_DECLARE_RIFT_FIXED_SIZE(Type) \
    namespace RiftSerializer { \
    namespace detail { \
        template<> struct is_rift_fixed_size_impl<Type> : std::true_type {}; \
    } /* namespace detail */ \
    } // namespace RiftSerializer

// Add explicit specializations for types that are not POD but we might treat as fixed-size
// Example: If you had a custom enum that wasn't declared as a Rift POD, but you know its fixed size.
// template<> struct is_rift_fixed_size<MyEnum> : std::true_type {};


// --- 3. Rift Variable-Size Trait ---
// A type is variable-size if its size in the serialized buffer can only be
// determined at runtime (e.g., strings, dynamic arrays).
// These types will require an entry in the OffsetTable.

    namespace detail {
        // Default: assume not variable-size unless specialized
        template<typename T>
        struct is_rift_variable_size_impl : std::false_type {};

        // Standard library containers that are inherently variable-sized
        template<typename T> struct is_rift_variable_size_impl<std::vector<T>> : std::true_type {};
        template<> struct is_rift_variable_size_impl<std::string> : std::true_type {};
        // Add other common variable-size types here (e.g., std::unique_ptr if its target is serialized, but be careful with ptrs)

    } // namespace detail

    template<typename T>
    struct is_rift_variable_size : detail::is_rift_variable_size_impl<T> {};

    // Macro to declare custom types as variable-size.
    // Use this for your custom dynamic containers or string types.
#define RIFT_SERIALIZER_DECLARE_RIFT_VARIABLE_SIZE(Type) \
    namespace RiftSerializer { \
    namespace detail { \
        template<> struct is_rift_variable_size_impl<Type> : std::true_type {}; \
    } /* namespace detail */ \
    } // namespace RiftSerializer


// --- Compile-Time Assertions for Mutually Exclusive Traits ---
// A type cannot be both fixed-size AND variable-size.
// This helps catch errors in schema definitions or trait declarations.

    template<typename T>
    struct RiftSerializerConceptCheck {
        static_assert(!(is_rift_fixed_size<T>::value&& is_rift_variable_size<T>::value),
            "Type cannot be both fixed-size and variable-size in RiftSerializer. Please check traits.");
    };

    // Explicitly instantiate RiftSerializerConceptCheck for common types to trigger the static_assert inside them.
    // This will cause a compile-time error if any of these types violate the mutual exclusivity rule.
    template struct RiftSerializerConceptCheck<int32>;
    template struct RiftSerializerConceptCheck<std::string>;
    template struct RiftSerializerConceptCheck<std::vector<float>>;


} // namespace RiftSerializer
