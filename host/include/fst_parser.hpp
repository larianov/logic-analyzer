#pragma once
#include <cstdint>
#include <span>
#include <sys/types.h>
#include <vector>
#include "../../embedded/include/config.hpp"
void fst_parse(std::span<const uint8_t> to_parse, const logic_an_input &config, const std::string &name_of_file);
