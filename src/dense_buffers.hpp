#pragma once

#include <memory>
#include <vector>

class DenseBufferArraySlice {
public:
    DenseBufferArraySlice() = delete;
    DenseBufferArraySlice(const std::shared_ptr<std::vector<char>>& buffer,
                          size_t offset, size_t size)
        : m_buffer(buffer), m_offset(offset), m_size(size) {}

    DenseBufferArraySlice(const DenseBufferArraySlice&) = default;
    DenseBufferArraySlice& operator=(const DenseBufferArraySlice&) = default;

    DenseBufferArraySlice(DenseBufferArraySlice&&) = default;
    DenseBufferArraySlice& operator=(DenseBufferArraySlice&&) = default;

    bool operator==(const DenseBufferArraySlice& other) const noexcept {
        return m_buffer == other.m_buffer;
    }

    const std::shared_ptr<std::vector<char>>& buffer() const noexcept {
        return m_buffer;
    }

    const char* data() const noexcept { return m_buffer->data() + m_offset; }
    size_t size() const noexcept { return m_size; }
    size_t offset() const noexcept { return m_offset; }

private:
    std::shared_ptr<std::vector<char>> m_buffer;
    size_t m_offset;
    size_t m_size;
};

class DenseBufferArray {
public:
    explicit DenseBufferArray(std::vector<char>&& buffer)
        : m_buffer(std::make_shared<std::vector<char>>(buffer)) {}
    explicit DenseBufferArray(const std::shared_ptr<std::vector<char>>& buffer)
        : m_buffer(buffer) {}
    explicit DenseBufferArray(std::shared_ptr<std::vector<char>>&& buffer)
        : m_buffer(buffer) {}

    DenseBufferArray(const DenseBufferArray&) = default;
    DenseBufferArray& operator=(const DenseBufferArray&) = default;

    DenseBufferArray(DenseBufferArray&& other) = default;
    DenseBufferArray& operator=(DenseBufferArray&& other) = default;

    const std::shared_ptr<std::vector<char>>& buffer() const noexcept {
        return m_buffer;
    }

    DenseBufferArraySlice slice(size_t offset, size_t size) const noexcept {
        return DenseBufferArraySlice(m_buffer, offset, size);
    }

private:
    std::shared_ptr<std::vector<char>> m_buffer;
};
