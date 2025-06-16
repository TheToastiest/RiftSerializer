// RiftSerializer/include/RiftSerializer/Builder.h
//
// Provides utilities for building serialized RiftSerializer data buffers at runtime.
// The RiftBufferBuilder class manages memory allocation and ensures data is written
// with correct alignment and endianness, preparing it for zero-copy deserialization.

#pragma once // Ensures this header is included only once per compilation unit.

#include "../Common/Common.h"    // For fixed-width types, endian conversion, RIFT_ASSERT, alignment utils
#include "../Types/Types.h"     // For RiftObjectHeader, OffsetTableEntry
#include "../Traits/Traits.h"    // For type traits like is_rift_fixed_size, is_rift_variable_size
#include <vector>      // For internal buffer management (dynamic array of bytes)
#include <string>      // For string serialization
#include <stdexcept>   // For exceptions in case of critical buffer errors
#include <limits>      // For std::numeric_limits

namespace RiftSerializer {

    // --- RiftBufferBuilder ---
    // A class for constructing RiftSerializer buffers.
    // It manages an internal dynamic byte buffer and provides methods to append
    // fixed-size fields, variable-sized data (like strings and arrays),
    // and manage object boundaries.
    class RiftBufferBuilder {
    public:
        // Default constructor: Initializes an empty buffer.
        RiftBufferBuilder() : m_current_offset(0) {
            // Reserve some initial capacity to reduce reallocations.
            m_buffer.reserve(1024);
        }

        // Constructor with initial capacity: Pre-allocates buffer memory.
        explicit RiftBufferBuilder(size_t initial_capacity) : m_current_offset(0) {
            m_buffer.reserve(initial_capacity);
        }

        // Get the current size of the data written to the buffer.
        size_t GetSize() const {
            return m_current_offset;
        }

        // Get a pointer to the start of the constructed buffer.
        const uint8* GetBufferPtr() const {
            return m_buffer.data();
        }

        // Reset the builder to an empty state, ready for a new buffer.
        void Reset() {
            m_current_offset = 0;
            m_buffer.clear();
        }

        // --- Core Buffer Write Operations ---

        // Ensures there is enough capacity in the buffer for 'bytes_needed'
        // and potentially resizes the buffer if necessary.
        void EnsureCapacity(size_t bytes_needed) {
            if (m_buffer.capacity() < m_current_offset + bytes_needed) {
                size_t new_capacity = std::max(m_buffer.capacity() * 2, m_current_offset + bytes_needed);
                m_buffer.reserve(new_capacity);
            }
        }

        // Advances the current write offset by `bytes`.
        // Used internally after writing data or for padding.
        void AdvanceOffset(size_t bytes) {
            RIFT_ASSERT(m_current_offset + bytes <= m_buffer.capacity(), "Buffer overflow during AdvanceOffset!");
            m_current_offset += bytes;
        }

        // Writes raw bytes directly to the buffer at the current offset.
        // Handles capacity and advances offset.
        void WriteRaw(const void* data, size_t size) {
            EnsureCapacity(size);
            std::memcpy(m_buffer.data() + m_current_offset, data, size);
            AdvanceOffset(size);
        }

        // Writes a value to the buffer, converting to Little Endian if necessary.
        // Assumes the current offset is already aligned for type T.
        template<typename T, std::enable_if_t<is_rift_fixed_size<T>::value && !std::is_same_v<T, bool>, int> = 0>
        void WriteValue(T value) {
            // Store the result of endian conversion in an l-value (a local variable)
            // so we can take its address for memcpy.
            const T le_value = to_little_endian(value);
            WriteRaw(&le_value, sizeof(T));
        }

        // Specialization for bool: writes as uint8.
        void WriteValue(bool value) {
            // Store the result of endian conversion in an l-value.
            const uint8 le_value = to_little_endian(value);
            WriteRaw(&le_value, sizeof(uint8));
        }

        // --- Alignment Utilities within the Builder ---

        // Pads the buffer to ensure the next write occurs at an 'alignment' boundary.
        // 'alignment' must be a power of 2.
        void PadToAlignment(size_t alignment) {
            size_t padding = (alignment - (m_current_offset % alignment)) % alignment;
            if (padding > 0) {
                EnsureCapacity(padding);
                // Zero-fill padding bytes for consistency and debugging.
                std::memset(m_buffer.data() + m_current_offset, 0, padding);
                AdvanceOffset(padding);
            }
        }

        // --- Serialization for Common Data Types ---

        // Begins a new RiftObject. Writes the header and reserves space for fixed data.
        // Returns the offset where the fixed data (after header) starts.
        // The `total_size` and `schema_id` in the header will be filled in later
        // when `EndObject` is called.
        size_t BeginObject(uint32 schema_id_val, uint32 version_flags_val) {
            PadToAlignment(alignof(RiftObjectHeader));
            size_t object_start_offset = m_current_offset;

            // Temporarily write a placeholder header. We'll fill in total_size later.
            RiftObjectHeader temp_header;
            temp_header.magic = to_little_endian(RIFT_MAGIC_NUMBER);
            temp_header.schema_id = to_little_endian(schema_id_val);
            temp_header.total_size = to_little_endian(0u); // Placeholder
            temp_header.version_flags = to_little_endian(version_flags_val);

            WriteRaw(&temp_header, sizeof(RiftObjectHeader));
            return object_start_offset;
        }

        // Ends a RiftObject. Fills in the correct total_size in the header.
        // Returns the total size of the object written.
        size_t EndObject(size_t object_start_offset) {
            size_t object_end_offset = m_current_offset;
            size_t total_object_size = object_end_offset - object_start_offset;

            RIFT_ASSERT(total_object_size <= std::numeric_limits<uint32>::max(),
                "RiftObject total_size exceeds uint32 limit!");

            // Update the total_size in the header.
            // We need to write the little-endian version back to the buffer.
            uint32 le_total_size = to_little_endian(static_cast<uint32>(total_object_size));
            std::memcpy(m_buffer.data() + object_start_offset + offsetof(RiftObjectHeader, total_size),
                &le_total_size, sizeof(uint32));

            return total_object_size;
        }

        // Stores the current offset of the buffer and returns it. Useful for back-patching.
        size_t GetCurrentOffset() const {
            return m_current_offset;
        }

        // Writes an OffsetTableEntry placeholder and returns its offset.
        // The actual offset and size will be filled in later.
        size_t ReserveOffsetTableEntry() {
            PadToAlignment(alignof(OffsetTableEntry));
            size_t entry_offset = m_current_offset;
            // Create an l-value for the blank entry to take its address.
            const OffsetTableEntry blank_entry{};
            WriteRaw(&blank_entry, sizeof(OffsetTableEntry));
            return entry_offset;
        }

        // Updates a previously reserved OffsetTableEntry with the actual data offset and size.
        void UpdateOffsetTableEntry(size_t entry_offset, uint32 data_offset, uint32 data_size) {
            RIFT_ASSERT(entry_offset + sizeof(OffsetTableEntry) <= m_current_offset,
                "UpdateOffsetTableEntry: Entry offset out of current bounds.");
            RIFT_ASSERT(is_aligned(entry_offset, alignof(OffsetTableEntry)),
                "UpdateOffsetTableEntry: Entry offset is unaligned!");

            OffsetTableEntry entry_to_write; // Create an l-value
            entry_to_write.offset = to_little_endian(data_offset);
            entry_to_write.size = to_little_endian(data_size);

            // Directly memcpy the updated entry back into the buffer.
            std::memcpy(m_buffer.data() + entry_offset, &entry_to_write, sizeof(OffsetTableEntry));
        }

        // --- Helper for Serializing Variable-Sized Data (String, Array) ---

        // Appends a string to the buffer, including its null terminator.
        // Returns the offset where the string data begins.
        uint32 AddString(const std::string& str) {
            // Strings usually don't need strict alignment for their data, but can be useful.
            // For simplicity, we'll align to 1 byte (no padding) here unless specified.
            // PadToAlignment(1); // Or a specific alignment if strings are stored aligned.
            uint32 string_data_offset = static_cast<uint32>(m_current_offset); // Current offset is string's start

            // Write string data + null terminator
            WriteRaw(str.data(), str.length());
            const char null_term = '\0'; // Create l-value for null terminator
            WriteRaw(&null_term, 1);     // Null terminator

            return string_data_offset;
        }

        // Appends an array of fixed-size elements to the buffer.
        // Returns the offset where the array data begins.
        template<typename T_Element, std::enable_if_t<is_rift_fixed_size<T_Element>::value, int> = 0>
        uint32 AddArray(const std::vector<T_Element>& arr) {
            PadToAlignment(alignof(T_Element)); // Align array start to element's natural alignment
            uint32 array_data_offset = static_cast<uint32>(m_current_offset); // Current offset is array's start

            for (const auto& element : arr) {
                WriteValue(element); // Uses WriteValue to handle endianness for each element
            }
            return array_data_offset;
        }

    private:
        std::vector<uint8> m_buffer; // The internal byte buffer where data is written
        size_t m_current_offset;     // The current write head position in the buffer

        // Disallow copying and assignment for builder objects for safety and performance.
        // If copying is needed, it should be explicit (e.g., through a CopyTo method).
        RiftBufferBuilder(const RiftBufferBuilder&) = delete;
        RiftBufferBuilder& operator=(const RiftBufferBuilder&) = delete;
        RiftBufferBuilder(RiftBufferBuilder&&) = default; // Allow moving
        RiftBufferBuilder& operator=(RiftBufferBuilder&&) = default; // Allow moving
    };

} // namespace RiftSerializer
