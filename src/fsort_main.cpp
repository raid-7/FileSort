#include "fsort.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cctype>

constexpr size_t DEFAULT_NUM_PAGES = 250;

[[noreturn]] static void usage() {
    std::cerr << "usage: fsort <input_file> <output_file> [num_pages]\n";
    exit(1);
}

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        usage();
    }

    fs::path input_path = argv[1];
    fs::path output_path = argv[2];
    size_t num_pages = DEFAULT_NUM_PAGES;
    if (argc > 3) {
        std::string_view num_pages_arg = argv[3];
        if (!std::all_of(num_pages_arg.begin(), num_pages_arg.end(), [](char c) { return c >= '0' && c <= '9'; })) {
            usage();
        }
        num_pages = std::atoll(num_pages_arg.data());
    }

    fs::path temp_dir = fs::current_path() / "fsort_tmp";
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    {
        FileSorter sorter{temp_dir, num_pages};
        sorter.sortFile(input_path, output_path);
    }
    fs::remove_all(temp_dir);

    return 0;
}
