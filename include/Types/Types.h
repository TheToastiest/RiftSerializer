// RiftSerializer/include/RiftSerializer/Types.h
//
// Defines core data structures for RiftSerializer, including
// the RiftObjectHeader and OffsetTableEntry. These are foundational
// for the binary format and zero-copy access.

#pragma once // Ensures this header is included only once per compilation unit.

#include "../common/Common.h" // For fixed-width types, endian conversion, and RIFT_ASSERT

namespace RiftSerializer {

    // --- RiftObjectHeader ---
    // This header precedes every serialized RiftObject. It contains crucial metadata
    // for identifying the format, its schema, total size, and versioning information.
    //
    // Designed to be a fixed size (16 bytes) and aligned to an 8-byte boundary
    // for optimal performance and to simplify alignment of subsequent data.
    struct alignas(8) RiftObjectHeader {
        // A magic number (e.g., 'RFS0') to quickly identify RiftSerializer data.
        // Stored in canonical Little Endian.
        uint32 magic;

        // Unique identifier (hash) for the object's specific schema version.
        // This allows for runtime schema validation and versioning.
        // Stored in canonical Little Endian.
        uint32 schema_id;

        // Total size of the complete serialized object instance in bytes,
        // including this header and all its fixed and variable-sized data.
        // This enables parsers to skip objects or validate buffer bounds.
        // Stored in canonical Little Endian.
        uint32 total_size;

        // A combined field for versioning and various flags.
        // This allows for compact storage of:
        // - Major Version (e.g., bits 8-15)
        // - Minor Version (e.g., bits 0-7)
        // - Specific flags (e.g., object has optional fields, custom serialization, etc. in bits 16-31)
        // Stored in canonical Little Endian.
        uint32 version_flags;

        // --- Compile-time checks for structural integrity of individual members ---
        // These static_asserts ensure that the compiler's interpretation of the struct
        // members matches the expected binary layout.
        static_assert(sizeof(magic) == 4, "RiftObjectHeader: 'magic' field size mismatch.");
        static_assert(sizeof(schema_id) == 4, "RiftObjectHeader: 'schema_id' field size mismatch.");
        static_assert(sizeof(total_size) == 4, "RiftObjectHeader: 'total_size' field size mismatch.");
        static_assert(sizeof(version_flags) == 4, "RiftObjectHeader: 'version_flags' field size mismatch.");
    };
    // --- Static assertions for the complete RiftObjectHeader struct (moved outside) ---
    // These checks ensure the overall size and alignment of the header.
    static_assert(sizeof(RiftObjectHeader) == 16, "RiftObjectHeader: incorrect total size (expected 16 bytes).");
    static_assert(alignof(RiftObjectHeader) == 8, "RiftObjectHeader: incorrect alignment (expected 8 bytes).");


    // --- OffsetTableEntry ---
    // This structure defines an entry within an Offset Table, which is used to
    // locate variable-sized data (like strings or dynamic arrays) within a RiftObject.
    //
    // It's designed to be 8 bytes and aligned to a 4-byte boundary.
    struct alignas(4) OffsetTableEntry {
        // The offset in bytes from the start of the current RiftObject to the
        // beginning of the variable-sized data this entry describes.
        // A value of 0 might indicate a null or empty field, depending on context.
        // Stored in canonical Little Endian.
        uint32 offset;

        // The 'size' of the variable data. Its interpretation depends on the field type:
        // - For strings: it's the character count (excluding null terminator).
        // - For arrays: it's the number of elements in the array.
        // - For other variable-sized data: it might be the total size in bytes.
        // Stored in canonical Little Endian.
        uint32 size;

        // --- Compile-time checks for structural integrity of individual members ---
        static_assert(sizeof(offset) == 4, "OffsetTableEntry: 'offset' field size mismatch.");
        static_assert(sizeof(size) == 4, "OffsetTableEntry: 'size' field size mismatch.");
    };
    // --- Static assertions for the complete OffsetTableEntry struct (moved outside) ---
    // These checks ensure the overall size and alignment of the entry.
    static_assert(sizeof(OffsetTableEntry) == 8, "OffsetTableEntry: incorrect total size (expected 8 bytes).");
    static_assert(alignof(OffsetTableEntry) == 4, "OffsetTableEntry: incorrect alignment (expected 4 bytes).");

} // namespace RiftSerializer
