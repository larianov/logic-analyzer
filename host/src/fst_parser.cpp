#include <iostream>
#include <cmath>
#include "fst_parser.hpp"
#include <cstdint>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <chrono>
extern "C"{
    #include "fstapi.h"
}

void fst_parse(std::span<const uint8_t> to_parse, const logic_an_input &config, const std::string& name_of_file) {
    std::ofstream file{name_of_file.c_str()};
    file<<"$date\n\t";
    auto time_utc = std::chrono::system_clock::now();
    auto time_local = std::chrono::zoned_time{std::chrono::current_zone(), time_utc};
    std::string formatted_time = std::format("{:%Y-%m-%d %H:%M:%S}", time_local);
    file << formatted_time << "\n" << "$end\n" << "$timescale\n\t";

    long double clk_div = double(125'000'000) / double(config.hz);
    auto int_part = static_cast<int16_t>(clk_div);
    auto pico_div = std::round((clk_div - int_part)*256.00);
    if (pico_div == 256) {
        int_part += 1;
        pico_div = 0;
    }
    
    long double f_real = double(125'000'000) / (int_part+(pico_div/(double)256));

    uint64_t femto_secs = (static_cast<uint64_t>(int_part) * 8'000'000ULL) + 
                          (static_cast<uint64_t>(pico_div) * 31'250ULL);
}