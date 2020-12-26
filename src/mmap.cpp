#include "mmap.hpp"


#include <filesystem>
#include <stdexcept>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>


static void throwSystemError() {
    int error_code = errno;
    errno = 0;
    throw std::system_error(std::make_error_code(static_cast<std::errc>(error_code)));
}

size_t getPageSize() {
    long res = sysconf(_SC_PAGESIZE);
    if (res == -1) {
        throwSystemError();
    }
    return res;
}

static size_t ceilToDivisible(size_t value, size_t divisor) {
    return ((value + divisor - 1) / divisor) * divisor;
}

static size_t floorToDivisible(size_t value, size_t divisor) {
    return (value / divisor) * divisor;
}

MemoryMappedFileBase::MemoryMappedFileBase(const fs::path& path, size_t offset, size_t size, bool readonly) {
    fs::file_status status = fs::symlink_status(path);
    if (!fs::is_regular_file(status)) {
        throw std::runtime_error("Not a regular file");
    }

    int fd = open(path.c_str(), readonly ? O_RDONLY : O_RDWR);
    if (fd == -1) {
        throwSystemError();
    }

    size_t page_size = getPageSize();
    size_t legal_offset = floorToDivisible(offset, page_size);
    size_t legal_size = ceilToDivisible(size + (offset - legal_offset), page_size);

    region_ = mmap(nullptr, legal_size, PROT_READ | (readonly ? 0 : PROT_WRITE), readonly ? MAP_PRIVATE : MAP_SHARED, fd, legal_offset);
    close(fd);

    if (region_ == MAP_FAILED) {
        throwSystemError();
    }

    size_ = legal_size;
    begin_ = static_cast<char*>(region_) + (offset - legal_offset);
    end_ = begin_ + size;
}

MemoryMappedFileBase::~MemoryMappedFileBase() noexcept {
    int res = munmap(region_, size_);
    if (res != 0) {
        errno = 0;
    }
}

void MemoryMappedFileBase::advice(MemoryMapUsage hint) {
    int advice;
    switch (hint) {
        case MemoryMapUsage::RANDOM:
            advice = MADV_RANDOM;
            break;
        case MemoryMapUsage::SEQUENTIAL:
            advice = MADV_SEQUENTIAL;
            break;
        default:
            advice = MADV_NORMAL;
    }

    int res = madvise(region_, size_, advice);
    if (res != 0) {
        throwSystemError();
    }
}
