#include <fcntl.h>
#include <ios>
#include <iostream>
#include <cmath>
#include "fst_parser.hpp"
#include <cstdint>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <chrono>
#include <sys/types.h>
#include <utility>
#include <vector>
extern "C" {
#include "fstapi.h"
}

void fst_parse(std::span<const uint8_t> to_parse, const logic_an_input &config, const std::string &name_of_file) {
    std::ofstream file{"debug.log"};
    auto *fst_object = fstWriterCreate(name_of_file.c_str(), 1);
    fstWriterSetTimescale(fst_object, -15);
    fstWriterSetScope(fst_object, FST_ST_VCD_MODULE, "GPIO", nullptr);
    std::vector<std::pair<uint8_t,fstHandle>>arr;
    for (uint8_t i{}; i < config.amm+1; i++) {
        arr.emplace_back(std::pair{255, fstWriterCreateVar(fst_object, FST_VT_VCD_WIRE, FST_VD_INPUT, 1,
                                         std::string("gpio" + std::to_string(config.channel+i)).c_str(), 0)});
    }
    uint64_t current_time_fs = 0;
    fstWriterSetUpscope(fst_object);
    fstWriterEmitTimeChange(fst_object, 0);
    
    for (auto v : to_parse) {
        file << std::format("{:b}", v);
        for (uint8_t i{}; i < config.amm+1; i++) {
            uint8_t bit = (v >> i) & 1;
            if (bit != arr[i].first) {
                fstWriterEmitTimeChange(fst_object, current_time_fs);
                if (bit == 0)
                    fstWriterEmitValueChange(fst_object, arr[i].second, "0");
                else
                    fstWriterEmitValueChange(fst_object, arr[i].second, "1");
                arr[i].first = bit;
            }
        }
        current_time_fs += step_fs;
    }
    fstWriterClose(fst_object);
}
