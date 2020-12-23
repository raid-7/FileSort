#pragma once

#include "mmap.hpp"
#include <filesystem>
#include <random>
#include <cmath>
#include <fstream>
#include <queue>
#include <compare>
#include <optional>

namespace fs = std::filesystem;


struct RegionLine {
    const char* region_pos;
    const char* region_end;
    std::string_view line;
};

std::strong_ordering operator <=>(const RegionLine& a, const RegionLine& b) {
    return a.line <=> b.line;
}

class FileSorter {
public:
    FileSorter(fs::path temp_dir, size_t num_pages_hint)
        : temp_dir_(std::move(temp_dir)), num_pages_hint_(num_pages_hint) {
        if (!fs::is_directory(temp_dir_)) {
            throw std::runtime_error("Temp directory is not a directory");
        }
        
        size_t page_size = getPageSize();
        region_size_ = page_size * num_pages_hint_;
        if (region_size_ >= (1ULL << 32ULL)) {
            throw std::runtime_error("The hint is too large");
        }
    }

    void sortFile(const fs::path& input_path, const fs::path& output_path) {
        if (!fs::is_regular_file(input_path)) {
            throw std::runtime_error("Input is not a regular file");
        }

        fs::file_status output_file_status = fs::status(output_path);
        if (fs::exists(output_file_status) && !fs::is_regular_file(output_file_status)) {
            throw std::runtime_error("Output exists and is not a regular file");
        }

        size_t size = fs::file_size(input_path);
        size_t num_regions = (size + region_size_ - 1) / region_size_;
        
        std::vector<fs::path> region_files;
        std::optional<std::string> last_partial_line = std::nullopt;
        fs::path region_file;
        for (size_t i = 0; i < num_regions; ++i) {
            if (region_file.empty()) {
                region_file = nextTempFile();
            }
            size_t offset = i * region_size_;
            bool written = sortRegion(
                input_path, offset, std::min(region_size_, size - offset), region_file, last_partial_line);
            if (written) {
                region_files.push_back(region_file);
                region_file = fs::path{};
            }
        }
        if (last_partial_line.has_value()) {
            fs::path last_region = nextTempFile();
            std::ofstream out{last_region};
            out << *last_partial_line << '\n';
            out.close();
            if (out.fail()) {
                throw std::runtime_error("Cannot write file");
            }
            region_files.push_back(last_region);
        }

        mergeRegions(region_files, output_path);
    }

private:
    fs::path nextTempFile() {
        fs::path res;
        do {
            res = temp_dir_ / nextRandomString(5);
        } while (fs::exists(res));
        return res;
    }

    std::string nextRandomString(size_t len) {
        std::string res(len, '\0');
        std::generate(res.begin(), res.end(), [this]() { return char_distribution_(random_engine_); });
        return res;
    }

    void mergeRegions(const std::vector<fs::path>& region_files, const fs::path& output_file) {
        if (!fs::exists(output_file)) {
            createFile(output_file);
        }
        std::vector<size_t> sizes;
        std::transform(region_files.begin(), region_files.end(), std::back_inserter(sizes),
                       static_cast<uintmax_t (*)(const fs::path&)>(fs::file_size));
        size_t size = std::accumulate(sizes.begin(),sizes.end(), size_t{});
        fs::resize_file(output_file, size);

        std::vector<std::unique_ptr<MemoryMappedFile<true>>> regions(region_files.size());
        std::transform(region_files.begin(), region_files.end(), regions.begin(), [](auto& p) {
            return std::make_unique<MemoryMappedFile<true>>(p, 0, fs::file_size(p));
        });

        std::priority_queue<RegionLine, std::vector<RegionLine>, std::greater<RegionLine>> heap;
        for (const auto& region_ptr : regions) {
            RegionLine region_line{region_ptr->begin(), region_ptr->end(), {}};
            if (region_line.region_pos != region_line.region_end) {
                region_ptr->advice(MemoryMapUsage::SEQUENTIAL);
                region_line.line = nextLine(region_line.region_pos, region_line.region_end);
                heap.push(region_line);
            }
        }

        size_t output_file_offset = 0;
        auto output = std::make_unique<MemoryMappedFile<false>>(
            output_file, output_file_offset, std::min(size - output_file_offset, region_size_));
        output->advice(MemoryMapUsage::SEQUENTIAL);
        char* output_pos = output->begin();
        while (!heap.empty()) {
            RegionLine region_line = heap.top();
            heap.pop();
            std::string_view line = region_line.line;
            ++region_line.region_pos; // skip \n
            if (region_line.region_pos < region_line.region_end) {
                region_line.line = nextLine(region_line.region_pos, region_line.region_end);
                heap.push(region_line);
            }

            const char* line_pos = line.data();
            size_t bytes_to_copy = std::min(static_cast<size_t>(output->end() - output_pos), line.size());
            while (bytes_to_copy > 0 || output_pos == output->end()) {
                std::copy_n(line_pos, bytes_to_copy, output_pos);

                output_pos += bytes_to_copy;
                line_pos += bytes_to_copy;
                if (output_pos == output->end()) {
                    output_file_offset += region_size_;
                    output = std::make_unique<MemoryMappedFile<false>>(
                        output_file, output_file_offset, std::min(size - output_file_offset, region_size_));
                    output->advice(MemoryMapUsage::SEQUENTIAL);
                    output_pos = output->begin();
                }

                bytes_to_copy = std::min(line.data() + line.size() - line_pos, output->end() - output_pos);
            }
            *(output_pos++) = '\n';
        }

        fs::resize_file(output_file, size - (output->end() - output_pos));
    }

    static bool sortRegion(const fs::path& input_file, size_t offset, size_t in_size,
                                                 const fs::path& output_file, std::optional<std::string>& partial_line) {
        if (!fs::exists(output_file)) {
            createFile(output_file);
        }
        size_t out_size = in_size + partial_line.value_or("").size();
        fs::resize_file(output_file, out_size);

        MemoryMappedFile<true> input {input_file, offset, in_size};
        input.advice(MemoryMapUsage::RANDOM);
        MemoryMappedFile<false> output {output_file, 0, out_size};
        output.advice(MemoryMapUsage::SEQUENTIAL);

        bool partial_line_processed = false;
        std::string first_line;
        std::vector<std::string_view> lines;
        for (const char* it = input.begin(); it < input.end(); ++it) {
            std::string_view line = nextLine(it, input.end());
            if (!partial_line_processed && partial_line.has_value()) {
                first_line = *partial_line + std::string(line.begin(), line.end());
                partial_line = std::nullopt;
                line = first_line;
                partial_line_processed = true;
            }
            if (it == input.end()) {
                partial_line = line;
            } else {
                lines.push_back(line);
            }
        }

        std::sort(lines.begin(), lines.end());

        char* pos = output.begin();
        for (std::string_view line : lines) {
            std::copy_n(line.data(), line.size(), pos);
            pos += line.size();
            if (pos < output.end()) {
                *(pos++) = '\n';
            }
        }

        if (!lines.empty()) {
            fs::resize_file(output_file, out_size - (output.end() - pos));
        }
        return !lines.empty();
    }

    static void createFile(const fs::path& file) {
        std::ofstream f(file);
    }

    static std::string_view nextLine(const char* &pos, const char* end) {
        const char* begin = pos;
        for (; pos < end; ++pos) {
            if (*pos == '\n') {
                return {begin, pos};
            }
        }
        return {begin, pos};
    }

private:
    std::mt19937_64 random_engine_{17};
    std::uniform_int_distribution<char> char_distribution_{'a', 'z'};
    fs::path temp_dir_;
    size_t num_pages_hint_;
    size_t region_size_;
};