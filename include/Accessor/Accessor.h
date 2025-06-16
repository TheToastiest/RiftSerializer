// RiftSerializer/include/RiftSerializer/Accessor.h
//
// Provides core templates and utilities for zero-copy access to serialized
// RiftSerializer data. These accessors handle endianness conversion,
// alignment, and bounds checking to ensure safe and efficient deserialization.

#pragma once // Ensures this header is included only once per compilation unit.

#include "../Common/Common.h" // For fixed-width types, endian conversion, RIFT_ASSERT, alignment utils
#include "../Types/Types.h"    // For RiftObjectHeader, OffsetTableEntry
#include <type_traits> // For std::enable_if, std::is_pointer, etc.

namespace RiftSerializer {

    // Forward declaration for schema-generated serialized types.
    // Your SchemaCompiler will generate structs like PlayerState_Serialized.
    // We provide a generic base here.
    struct GenericRiftObject_Serialized; // Base for all generated serialized structs


    // --- Base RiftBufferView ---
    // This is the fundamental view class for any serialized RiftObject.
    // It provides basic validation and access to the common RiftObjectHeader fields.
    // All specific generated schema views (e.g., PlayerState_View) will likely
    // contain or inherit from a pointer to their _Serialized type, which
    // itself starts with this header.
    struct RiftBufferViewBase {
    protected:
        const uint8* m_buffer_start;
        const RiftObjectHeader* m_header;

    public:
        // Constructor performs initial validation common to all RiftObjects.
        // It takes a raw pointer to the start of the serialized buffer.
        explicit RiftBufferViewBase(const void* buffer_start)
            : m_buffer_start(static_cast<const uint8*>(buffer_start))
        {
            // Assert that the buffer pointer is valid.
            RIFT_ASSERT(m_buffer_start != nullptr, "RiftBufferViewBase: Buffer start pointer is null.");

            // Attempt to cast to RiftObjectHeader for initial checks.
            // We assume the header is always at the beginning and aligned.
            m_header = reinterpret_cast<const RiftObjectHeader*>(m_buffer_start);

            // Validate magic number. This confirms it's a RiftSerializer buffer.
            // We read it from the buffer and convert to host endianness for comparison.
            RIFT_ASSERT(from_little_endian(m_header->magic) == RIFT_MAGIC_NUMBER,
                "RiftBufferViewBase: Invalid RiftSerializer magic number.");

            // The total_size field in the header gives us the total extent of this object.
            // This is crucial for bounds checking.
            // We need to convert it from canonical Little Endian to host endianness.
            uint32 actual_total_size = from_little_endian(m_header->total_size);
            RIFT_ASSERT(actual_total_size >= sizeof(RiftObjectHeader),
                "RiftBufferViewBase: Corrupt total_size in header (too small).");
            // We can't do full bounds checking here without knowing the buffer's capacity,
            // but individual accessors will perform more granular checks.
        }

        // Accessors for common header fields (converted to host endianness).
        uint32 GetSchemaId() const { return from_little_endian(m_header->schema_id); }
        uint32 GetTotalSize() const { return from_little_endian(m_header->total_size); }
        uint32 GetVersionFlags() const { return from_little_endian(m_header->version_flags); }

        // Utility to get a pointer relative to the buffer start.
        // This is the underlying mechanism for all pointer-based access.
        const uint8* GetPtrAtOffset(uint32 offset, size_t size_needed = 1) const {
            // Perform bounds checking to ensure access is within the object's total size.
            RIFT_ASSERT(offset + size_needed <= GetTotalSize(),
                "RiftBufferViewBase: Access out of bounds for object data.");
            return m_buffer_start + offset;
        }
    };

    // --- Accessors for Fixed-Size Fields ---
    // These functions provide safe, endian-correct access to fixed-size data members.
    // They are used internally by the generated _View structs.

    // Reads a fixed-size value from the buffer at the given offset,
    // converting it from Little Endian to host endianness.
    // T must be an integral or floating-point type declared as Rift POD.
    template<typename T, typename std::enable_if_t<is_rift_pod<T>::value && !std::is_pointer<T>::value, int> = 0>
    inline T GetFixedField(const uint8* buffer_start, uint32 offset) {
        // Get a pointer to the location of the value in the buffer.
        const T* ptr = reinterpret_cast<const T*>(buffer_start + offset);

        // Assert alignment for the type. Crucial for performance and correctness.
        RIFT_ASSERT(is_aligned(reinterpret_cast<size_t>(ptr), alignof(T)),
            "GetFixedField: Unaligned access detected!");

        // Read the value and convert endianness.
        // This is safe because T is trivially copyable/POD.
        return from_little_endian(*ptr);
    }

    // Overload for bool, which is serialized as uint8.
    inline bool GetFixedField(const uint8* buffer_start, uint32 offset) {
        const uint8* ptr = reinterpret_cast<const uint8*>(buffer_start + offset);
        RIFT_ASSERT(is_aligned(reinterpret_cast<size_t>(ptr), alignof(uint8)),
            "GetFixedField<bool>: Unaligned access detected!");
        return from_little_endian(*ptr) != 0; // Convert uint8 to bool
    }


    // --- RiftStringView ---
    // A zero-copy view for serialized strings.
    // It provides methods to access the string data and its length without copying.
    class RiftStringView {
    private:
        const char* m_data;
        uint32 m_length; // Number of characters, excluding null terminator

    public:
        // Construct from a raw buffer pointer and the length.
        // The data should be null-terminated in the buffer for C-string compatibility.
        // The length here is the character count.
        RiftStringView(const void* data_ptr, uint32 length)
            : m_data(static_cast<const char*>(data_ptr)), m_length(length)
        {
            RIFT_ASSERT(m_data != nullptr, "RiftStringView: Null data pointer.");
            // We assume the null terminator exists just after length chars.
            // It's good practice to ensure this during serialization.
            // A more robust check might verify m_data[m_length] == '\0' if total buffer known.
        }

        // Get the length of the string (number of characters).
        uint32 length() const { return m_length; }

        // Get a pointer to the raw string data (guaranteed null-terminated if serialized correctly).
        const char* c_str() const { return m_data; }

        // Convert to std::string (this will involve a copy, but is convenient).
        std::string to_string() const { return std::string(m_data, m_length); }

        // Check if the string view is empty.
        bool empty() const { return m_length == 0; }
    };


    // --- RiftArrayView ---
    // A zero-copy view for serialized arrays of fixed-size elements.
    // This allows iteration over array elements without copying the whole array.
    template<typename T_Element>
    class RiftArrayView {
    private:
        const uint8* m_array_start_ptr;
        uint32 m_element_count;

    public:
        // Construct from a raw buffer pointer (to the first element) and element count.
        RiftArrayView(const void* array_data_ptr, uint32 element_count)
            : m_array_start_ptr(static_cast<const uint8*>(array_data_ptr)),
            m_element_count(element_count)
        {
            RIFT_ASSERT(m_array_start_ptr != nullptr, "RiftArrayView: Null array data pointer.");
            // RIFT_ASSERT for T_Element must be Rift POD or have defined serialized size
            static_assert(is_rift_fixed_size<T_Element>::value || is_rift_pod<T_Element>::value,
                "RiftArrayView: Element type must be fixed-size or Rift POD.");
        }

        // Get the number of elements in the array.
        uint32 size() const { return m_element_count; }

        // Check if the array is empty.
        bool empty() const { return m_element_count == 0; }

        // Access an element by index. Performs bounds checking.
        // This returns the actual C++ type (T_Element), handling endianness.
        // If T_Element is a complex fixed-size struct (e.g., Vec3_Serialized),
        // it will return that directly. If it's a primitive (int, float), it
        // returns the host-endian value.
        T_Element operator[](uint32 index) const {
            RIFT_ASSERT(index < m_element_count, "RiftArrayView: Index out of bounds.");

            // Calculate offset to the element.
            uint32 element_offset = index * sizeof(T_Element);

            // Get a pointer to the element's raw serialized data.
            const uint8* element_ptr = m_array_start_ptr + element_offset;

            // Perform alignment check for the element.
            RIFT_ASSERT(is_aligned(reinterpret_cast<size_t>(element_ptr), alignof(T_Element)),
                "RiftArrayView: Unaligned element access detected!");

            // Read and return the value, handling endianness for PODs.
            // This relies on the GetFixedField/from_little_endian logic for primitives.
            // For custom structs declared as RIFT_SERIALIZER_DECLARE_RIFT_POD,
            // this will return a copy of the _Serialized struct, which callers
            // would then use their own accessors on.
            if constexpr (is_rift_pod<T_Element>::value) {
                return GetFixedField<T_Element>(m_array_start_ptr, element_offset);
            }
            else {
                // For fixed-size non-POD elements that require specific constructors,
                // this might need a different handling or a dedicated view.
                // For now, this assumes it can be bitwise copied.
                T_Element value;
                std::memcpy(&value, element_ptr, sizeof(T_Element));
                return value;
            }
        }

        // Iterators for range-based for loops (optional but good practice).
        // These would return proxy objects or raw pointers, depending on desired behavior.
        // For simplicity, we'll omit full iterator implementations here for brevity,
        // but they are a common addition for array-like views.
    };


    // --- Helper for Reading Variable-Sized Field Offsets ---
    // Used by generated schema views to resolve variable-sized data.
    // 'object_base_ptr' is the start of the current RiftObject.
    // 'offset_table_offset' is the offset from object_base_ptr to the start of the OffsetTable.
    // 'entry_index' is the index within the OffsetTable for the desired field.
    inline const OffsetTableEntry* GetOffsetTableEntry(const uint8* object_base_ptr,
        uint32 offset_table_offset,
        uint32 entry_index,
        uint32 expected_num_entries) {
        RIFT_ASSERT(object_base_ptr != nullptr, "GetOffsetTableEntry: Null object base pointer.");
        RIFT_ASSERT(offset_table_offset > 0, "GetOffsetTableEntry: Offset table offset must be positive.");
        RIFT_ASSERT(entry_index < expected_num_entries, "GetOffsetTableEntry: Entry index out of bounds.");

        // Calculate the start of the offset table.
        const OffsetTableEntry* table_start = reinterpret_cast<const OffsetTableEntry*>(
            object_base_ptr + offset_table_offset
            );

        // Perform alignment check for the table start.
        RIFT_ASSERT(is_aligned(reinterpret_cast<size_t>(table_start), alignof(OffsetTableEntry)),
            "GetOffsetTableEntry: Offset table is unaligned!");

        // Check if accessing beyond the expected number of entries.
        RIFT_ASSERT(entry_index * sizeof(OffsetTableEntry) < expected_num_entries * sizeof(OffsetTableEntry),
            "GetOffsetTableEntry: Accessing beyond expected number of offset table entries.");

        // Access the specific entry and convert from Little Endian.
        // No need to convert here, as we return the raw entry; conversion happens on field access.
        return table_start + entry_index;
    }


    // --- GenericRiftObject_Serialized (Base for generated serialized structs) ---
    // This struct will be the base for all generated _Serialized structs (e.g., PlayerState_Serialized).
    // It ensures that every generated serialized struct starts with the RiftObjectHeader.
    struct GenericRiftObject_Serialized {
        RiftObjectHeader header;
        // Followed by fixed-size data and then the offset table (logically)
    };

    // Static assert to ensure GenericRiftObject_Serialized starts with the header
    static_assert(offsetof(GenericRiftObject_Serialized, header) == 0,
        "GenericRiftObject_Serialized must start with RiftObjectHeader.");

} // namespace RiftSerializer
