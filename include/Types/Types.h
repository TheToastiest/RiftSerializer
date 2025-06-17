// RiftSerializer/include/RiftSerializer/Types.h
#pragma once

#include "../Common/Common.h"

namespace RiftSerializer {

    // A magic number to identify RiftSerializer buffers.
    // 'RFS1' in Little Endian (version 1)
    constexpr uint32 RIFT_MAGIC_NUMBER = 0x31534652;

    // --- RiftObjectHeader ---
    // This fixed-size header (16 bytes) precedes every serialized RiftObject.
    // It is aligned to 8 bytes for performance.
    struct alignas(8) RiftObjectHeader {
        uint32 magic;         // Magic number, e.g., 'RFS1'
        uint32 schema_id;     // Hash of the object's schema definition
        uint32 total_size;    // Total size of the object in bytes, including this header
        uint32 version_flags; // For versioning and feature flags
    };
    static_assert(sizeof(RiftObjectHeader) == 16, "RiftObjectHeader must be 16 bytes.");
    static_assert(alignof(RiftObjectHeader) == 8, "RiftObjectHeader must be 8-byte aligned.");


    // --- OffsetTableEntry ---
    // This struct (8 bytes) points to variable-sized data within the object's buffer.
    // It is aligned to 4 bytes.
    struct alignas(4) OffsetTableEntry {
        uint32 offset; // Offset from the start of the object to the variable data
        uint32 size;   // Size of the data (e.g., character count for strings, element count for arrays)
    };
    static_assert(sizeof(OffsetTableEntry) == 8, "OffsetTableEntry must be 8 bytes.");
    static_assert(alignof(OffsetTableEntry) == 4, "OffsetTableEntry must be 4-byte aligned.");

} // namespace RiftSerializer