#pragma once

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

size_t getPageSize();

enum class MemoryMapUsage {
    RANDOM, SEQUENTIAL
};

class MemoryMappedFileBase {
public:
    explicit MemoryMappedFileBase(const fs::path& path, size_t offset, size_t size, bool readonly);

    void advice(MemoryMapUsage hint);

    MemoryMappedFileBase(const MemoryMappedFileBase&) = delete;
    MemoryMappedFileBase& operator =(const MemoryMappedFileBase&) = delete;
    MemoryMappedFileBase(MemoryMappedFileBase&&) = delete;
    MemoryMappedFileBase& operator =(MemoryMappedFileBase&&) = delete;

    ~MemoryMappedFileBase() noexcept;

protected:
    char* begin_;
    char* end_;

private:
    void* region_;
    size_t size_;
};


template <bool readonly>
class MemoryMappedFile : public MemoryMappedFileBase {
public:
    explicit MemoryMappedFile(const fs::path& path, size_t offset, size_t size)
        : MemoryMappedFileBase(path, offset, size, readonly) {}

    char* begin() requires (!readonly) {
        return begin_;
    }

    char* end() requires (!readonly) {
        return end_;
    }

    const char* begin() const {
        return begin_;
    }

    const char* end() const {
        return end_;
    }
};
