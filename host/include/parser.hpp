#pragma once
#include "../../embedded/include/config.hpp"
#include <cstdint>
#include <string_view>

enum class state_of_parse : uint8_t {
    PARSING_CHANNEL,
    PARSING_HZ,
    PARSING_SAMPLES,
};
logic_an_input parse_cin();

bool confirm_channel(std::string_view line, logic_an_input &input, std::string_view& error_message);

bool confirm_freq(std::string_view line, logic_an_input &input, std::string_view& error_message);

bool confirm_samples(std::string_view line, logic_an_input &input, std::string_view& error_message);


bool confirm_vcd(std::string_view line, std::string_view& error_message);