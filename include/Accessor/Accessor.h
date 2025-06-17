// RiftSerializer/include/RiftSerializer/Accessor.h
// FINAL, COMPLETE VERSION - 2025-06-17

#pragma once

#include "../Types/Types.h"
#include "../Traits/Traits.h"
#include <string_view>
#include <concepts> // For std::concept

namespace RiftSerializer {

    // --- RiftBufferViewBase ---
    // A base class for generated _View structs. It provides common functionality
    // like validating the buffer and accessing the header.
    class RiftBufferViewBase {
    protected:
        const uint8* m_buffer_start;
        const RiftObjectHeader* m_header;

    public:
        // Constructor validates the buffer and header upon creation.
        explicit RiftBufferViewBase(const void* buffer)
            : m_buffer_start(static_cast<const uint8*>(buffer)),
            m_header(static_cast<const RiftObjectHeader*>(buffer))
        {
            RIFT_ASSERT(m_buffer_start != nullptr, "Buffer pointer cannot be null.");
            RIFT_ASSERT(is_aligned(m_buffer_start, alignof(RiftObjectHeader)), "Buffer is not aligned for RiftObjectHeader.");
            RIFT_ASSERT(from_little_endian(m_header->magic) == RIFT_MAGIC_NUMBER, "Invalid RiftSerializer magic number.");
            RIFT_ASSERT(from_little_endian(m_header->total_size) >= sizeof(RiftObjectHeader), "Buffer total_size is corrupt.");
        }

        uint32 GetSchemaId() const { return from_little_endian(m_header->schema_id); }
        uint32 GetTotalSize() const { return from_little_endian(m_header->total_size); }

        const uint8* GetPtrAtOffset(uint32 offset, size_t size_needed = 1) const {
            RIFT_ASSERT(offset + size_needed <= GetTotalSize(), "Memory access out of object bounds.");
            return m_buffer_start + offset;
        }
    };

    // --- RiftStringView ---
    // A simple, zero-copy string view.
    class RiftStringView {
    private:
        const char* m_data;
        uint32 m_length; // Length *without* null terminator.

    public:
        RiftStringView(const char* data, uint32 length) : m_data(data), m_length(length) {}

        const char* data() const { return m_data; }
        uint32 size() const { return m_length; }
        bool empty() const { return m_length == 0; }

        // Provides compatibility with std::string and other libraries.
        std::string_view to_std_string_view() const { return { m_data, m_length }; }
        std::string to_std_string() const { return { m_data, m_length }; }
    };

    // This C++20 concept checks if a type T has a constructor T(const void*).
    // All our generated _View classes will have this.
    template <typename T>
    concept Viewable = requires(const void* p) { T(p); };

    // --- RiftArrayView ---
    // A zero-copy view for reading an array of elements.
    template<typename T_Serialized, typename T_View>
        requires Viewable<T_View> || std::is_same_v<T_Serialized, T_View>
    class RiftArrayView {
    private:
        const uint8* m_array_start;
        uint32 m_element_count;

    public:
        RiftArrayView(const void* array_data, uint32 element_count)
            : m_array_start(static_cast<const uint8*>(array_data)), m_element_count(element_count)
        {
            static_assert(is_rift_fixed_size<T_Serialized>::value, "RiftArrayView can only be used with fixed-size element types.");
            if (m_element_count > 0) {
                RIFT_ASSERT(m_array_start != nullptr, "Array data pointer is null for non-empty array.");
                RIFT_ASSERT(is_aligned(m_array_start, alignof(T_Serialized)), "Array data is not aligned for its element type.");
            }
        }

        uint32 size() const { return m_element_count; }

        // Access an element by index.
        T_View at(uint32 index) const {
            RIFT_ASSERT(index < m_element_count, "Array index out of bounds.");
            const void* element_ptr = m_array_start + (index * sizeof(T_Serialized));

            if constexpr (std::is_same_v<T_Serialized, T_View>) {
                // For primitive types, copy the value and convert its endianness
                T_Serialized value;
                std::memcpy(&value, element_ptr, sizeof(T_Serialized));
                return from_little_endian(value);
            }
            else {
                // For custom structs, construct a View that points to the data
                return T_View(element_ptr);
            }
        }

        // operator[] for convenience
        T_View operator[](uint32 index) const {
            return at(index);
        }
    };
} // namespace RiftSerializer