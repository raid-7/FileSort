#include "mmap.hpp"
#include <iostream>
#include <fstream>
#include <random>

[[noreturn]] static void usage() {
    std::cerr << "usage: fgen <input_file> <max_line_len> <file_size>\n";
    exit(1);
}

void generate(char* begin, char* end, size_t max_line_len) {
    auto micro_from_epoch =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
    std::mt19937_64 random_engine(micro_from_epoch.count());
    std::uniform_int_distribution<size_t> length_distribution{1, max_line_len};
    std::uniform_int_distribution<char> char_distribution{'/', '~'};

    while (begin < end) {
        size_t len = std::min(static_cast<size_t>(end - begin), length_distribution(random_engine));
        std::generate(begin, begin + len, [&]() { return char_distribution(random_engine); });
        begin += len;
        if (begin < end) {
            *(begin++) = '\n';
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        usage();
    }

    fs::path output_path = argv[1];

    std::string_view line_len_arg = argv[2];
    std::string_view file_size_arg = argv[3];
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    if (!std::all_of(line_len_arg.begin(), line_len_arg.end(), is_digit)) {
        usage();
    }
    if (!std::all_of(file_size_arg.begin(), file_size_arg.end(), is_digit)) {
        usage();
    }

    size_t max_line_len = std::atoll(line_len_arg.data());
    size_t file_size = std::atoll(file_size_arg.data());

    if (!fs::exists(output_path)) {
        std::ofstream { output_path };
    }
    fs::resize_file(output_path, file_size);

    MemoryMappedFile<false> output {output_path, 0, file_size};
    output.advice(MemoryMapUsage::SEQUENTIAL);
    generate(output.begin(), output.end(), max_line_len);

    return 0;
}
