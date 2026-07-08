#include "vcd_parser.hpp"
#include <cstdint>
#include <fstream>
#include <span>
void vcd_parse(std::span<const uint8_t> to_parse) {
    std::ofstream file{"output.vcd"};
    for (auto v : to_parse) {
        file << +v << ' ';
    }
    file << '\n';
}