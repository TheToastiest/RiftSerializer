// RiftSerializer/include/RiftSerializer/Builder.h
// FINAL, COMPLETE VERSION - 2025-06-17

#pragma once

#include "../Accessor/Accessor.h"
#include <vector>
#include <string>

namespace RiftSerializer {

    class RiftBufferBuilder {
    public:
        explicit RiftBufferBuilder(size_t initial_capacity = 1024) {
            m_buffer.reserve(initial_capacity);
        }

        const uint8* GetBufferPointer() const { return m_buffer.data(); }
        size_t GetCurrentSize() const { return m_buffer.size(); }
        void Reset() { m_buffer.clear(); }

        void WriteRaw(const void* data, size_t size) {
            if (!data || size == 0) return;
            const auto* bytes = static_cast<const uint8*>(data);
            m_buffer.insert(m_buffer.end(), bytes, bytes + size);
        }

        void WriteAt(size_t offset, const void* data, size_t size) {
            RIFT_ASSERT(offset + size <= m_buffer.size(), "WriteAt would write out of bounds.");
            if (data && size > 0) {
                std::memcpy(m_buffer.data() + offset, data, size);
            }
        }

        size_t Reserve(size_t size) {
            PadToAlignment(8);
            size_t offset = m_buffer.size();
            m_buffer.resize(offset + size);
            return offset;
        }

        void PadToAlignment(size_t alignment) {
            size_t current_size = m_buffer.size();
            size_t padding = (alignment - (current_size % alignment)) % alignment;
            if (padding > 0) {
                m_buffer.resize(current_size + padding, 0);
            }
        }

        size_t BeginObject() {
            PadToAlignment(alignof(RiftObjectHeader));
            return GetCurrentSize();
        }

        void EndObject(size_t object_start_offset, uint32 schema_id) {
            RIFT_ASSERT(is_aligned(m_buffer.data() + object_start_offset, alignof(RiftObjectHeader)), "Object start is not aligned.");
            auto* header = reinterpret_cast<RiftObjectHeader*>(m_buffer.data() + object_start_offset);

            header->magic = to_little_endian(RIFT_MAGIC_NUMBER);
            header->schema_id = to_little_endian(schema_id);
            header->total_size = to_little_endian(static_cast<uint32>(GetCurrentSize() - object_start_offset));
            header->version_flags = to_little_endian(static_cast<uint32>(0)); // Reserved for future use
        }

        template <typename T>
        uint32_t AddArray(const std::vector<T>& arr) {
            if (arr.empty()) return 0;
            static_assert(is_rift_fixed_size<T>::value, "AddArray requires fixed-size types.");

            PadToAlignment(alignof(T));
            uint32_t start_offset = static_cast<uint32>(GetCurrentSize());
            WriteRaw(arr.data(), arr.size() * sizeof(T));
            return start_offset;
        }

        uint32_t AddString(const std::string& str) {
            if (str.empty()) return 0;
            uint32_t start_offset = static_cast<uint32>(GetCurrentSize());
            WriteRaw(str.data(), str.length() + 1); // Write string data AND null terminator
            return start_offset;
        }
    private:
        std::vector<uint8> m_buffer;
    };

} // namespace RiftSerializer