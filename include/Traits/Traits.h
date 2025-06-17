// RiftSerializer/include/RiftSerializer/Traits.h
#pragma once

#include "../Common/Common.h"
#include <type_traits>
#include <string>
#include <vector>

// --- GLM Includes ---
// The serializer needs to know about the types it will handle.
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace RiftSerializer {

    // --- 1. Rift POD (Plain Old Data) Trait ---
    // A type is a Rift POD if it is standard-layout and trivially-copyable.
    // This is the primary trait that allows for memcpy optimization.

    namespace detail {
        // Default to false. A type must be explicitly declared as a Rift POD.
        template<typename T>
        struct is_rift_pod_impl : std::false_type {};

        // Specializations for built-in types
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
        template<> struct is_rift_pod_impl<bool> : std::true_type {};
    } // namespace detail

    template<typename T>
    struct is_rift_pod : detail::is_rift_pod_impl<std::remove_cv_t<T>> {};

    // Macro to declare custom types as Rift PODs.
    // This is the main interface for users to register their structs.
#define RIFT_SERIALIZER_DECLARE_RIFT_POD(Type) \
    namespace detail { \
        template<> struct is_rift_pod_impl<Type> : std::true_type {}; \
    } \
    static_assert(std::is_standard_layout<Type>::value, \
        #Type " declared as Rift POD but is not standard layout. Rift PODs must be standard layout."); \
    static_assert(std::is_trivially_copyable<Type>::value, \
        #Type " declared as Rift POD but is not trivially copyable. Rift PODs must be trivially copyable.");

    // --- Declare common 3rd-party library types as PODs ---
    RIFT_SERIALIZER_DECLARE_RIFT_POD(glm::vec2)
        RIFT_SERIALIZER_DECLARE_RIFT_POD(glm::vec3)
        RIFT_SERIALIZER_DECLARE_RIFT_POD(glm::vec4)
        RIFT_SERIALIZER_DECLARE_RIFT_POD(glm::quat)
        RIFT_SERIALIZER_DECLARE_RIFT_POD(glm::mat3)
        RIFT_SERIALIZER_DECLARE_RIFT_POD(glm::mat4)


        // --- 2. Rift Fixed-Size Trait ---
        // A type is fixed-size if its size is known at compile time.
        // All PODs are fixed-size.
        template<typename T>
    struct is_rift_fixed_size : is_rift_pod<std::remove_cv_t<T>> {};


    // --- 3. Rift Variable-Size Trait ---
    // A type is variable-size if its size can only be known at runtime.
    namespace detail {
        template<typename T>
        struct is_rift_variable_size_impl : std::false_type {};

        // Specializations for common STL containers
        template<typename T>
        struct is_rift_variable_size_impl<std::vector<T>> : std::true_type {};
        template<>
        struct is_rift_variable_size_impl<std::string> : std::true_type {};
    } // namespace detail

    template<typename T>
    struct is_rift_variable_size : detail::is_rift_variable_size_impl<std::remove_cv_t<T>> {};


    // --- Compile-Time Concept Check ---
    // Ensures a type isn't ambiguous (e.g., both fixed and variable size).
    template<typename T>
    inline constexpr bool check_type_concepts() {
        static_assert(!(is_rift_fixed_size<T>::value && is_rift_variable_size<T>::value),
            "Type cannot be both fixed-size and variable-size.");
        return true;
    }

} // namespace RiftSerializer