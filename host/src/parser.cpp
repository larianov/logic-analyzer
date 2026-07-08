#include <cmath>
#include "parser.hpp"
#include "iostream"
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace aur {
bool getline(std::span<char> buffer) {
    if (buffer.size() == 0)
        return false;
    for (unsigned int i{}; i < buffer.size() - 1; i++) {
        char ch;
        if (!std::cin.get(ch)) {
            return false;
        }
        if (ch == '\r' || ch == '\n') {
            std::cout << "\r\n";
            buffer[i] = '\0';
            return true;
        }
        buffer[i] = ch;
    }
    buffer[0] = '\0';
    std::cout << "\r\n";
    return false;
}
} // namespace aur

static uint32_t from_string_to_hz(std::string_view input, std::string_view &error) {
    double digit{};
    auto [ptr, er]{std::from_chars(input.data(), input.data() + input.size(), digit)};
    if (er != std::errc()) {
        error = "Frequency: not found digits into input";
        return 0;
    }
    input.remove_prefix(ptr - input.data());
    while (!input.empty() && isspace(input.front())) {
        input.remove_prefix(1);
    }

    if (input.size() == 3 && tolower(input[0]) == 'm' && tolower(input[1]) == 'h' && tolower(input[2]) == 'z') {
        double result = digit * 1'000'000.0;
        if (result > 200000000.0) {
            error = "Frequency: overfilling";
            return 0;
        }
        return static_cast<uint32_t>(std::llround(result));
    }
    if (input.size() == 3 && tolower(input[0]) == 'k' && tolower(input[1]) == 'h' && tolower(input[2]) == 'z') {
        double result = digit * 1'000.0;
        if (result > 200000000.0) {
            error = "Frequency: overfilling";
            return 0;
        }
        return static_cast<uint32_t>(std::llround(result));
    }
    if (input.size() == 2 && tolower(input[0]) == 'h' && tolower(input[1]) == 'z') {
        if (digit > 200000000.0) {
            error = "Frequency: overfilling";
            return 0;
        }
        return static_cast<uint32_t>(std::llround(digit));
    }

    error = "Frequency: not found suffix";
    return 0;
}

logic_an_input parse_cin() {
    state_of_parse st = state_of_parse::PARSING_CHANNEL;
    std::string_view input;
    std::array<char, 16> buffer{};
    logic_an_input result{};
    while (true) {
        switch (st) {
        case state_of_parse::PARSING_CHANNEL: {
            std::cout << "What channel do you want to use?\r\n";
            if (!aur::getline(buffer)) {
                std::cout << "Your input is wrong, try again\r\n";
                break;
            }
            input = std::string_view{buffer.data()};
            auto [ptr, error] = std::from_chars(input.begin(), input.end(), result.channel);
            if (error == std::errc() && ptr == input.end()) {
                if (result.channel > 28) {
                    std::cout << "Channels are from 0 to 28. Try again please.\r\n";
                    continue;
                }
                st = state_of_parse::PARSING_HZ;
                break;
            }
            std::cout << "Your input is wrong, try again\r\n";
            break;
        }
        case state_of_parse::PARSING_HZ:
            std::cout << "Enter the sampling frequency.\r\n"
                      << "Format: <number> <unit>\r\n"
                      << "Supported units: Hz, kHz, MHz\r\n"
                      << "Examples: 100 Hz, 13 khz, 20MHz\r\n"
                      << "Allowed hz from 1Hz to 200MHz\r\n";
            if (!aur::getline(buffer)) {
                std::cout << "Your input is wrong, try again\r\n";
                break;
            }
            input = std::string_view{buffer.data()};
            result.hz = from_string_to_hz(input, input);
            if (result.hz == 0) {
                std::cout << "Your input contains an error.\r\nIt can be not correct sufix or to big num, please try "
                             "again\r\n";
                continue;
            }
            st = state_of_parse::PARSING_SAMPLES;
            break;
        case state_of_parse::PARSING_SAMPLES:
            std::cout << "How much samples do you want to use?\r\nMaximum is: 1'000'000\r\n";
            if (!aur::getline(buffer)) {
                std::cout << "Your input is wrong, try again\r\n";
                break;
            }
            input = std::string_view{buffer.data()};
            auto [ptr, error] = std::from_chars(input.begin(), input.end(), result.samples);
            if (error == std::errc() && ptr == input.end()) {
                if (result.samples == 0 || result.samples > 1000000) {
                    std::cout << "Your input contains an error.\r\nIt can not be a negative or zero\r\n";
                    continue;
                }
                return result;
            }
            std::cout << "Your input is wrong, try again\r\n";
            break;
        }
        if (std::cin.eof()) {
            std::cout << "You pressed unavaliable key-bind for this CLI, by default we use channel 15, Freq: 100 Mhz, "
                         "and 100,000 samples, sorry for inconvient behaviour\r\n";
            result = {.msg = 6, .channel = 15, .hz = 100000000, .samples = 100000};
            return result;
        }
    }
}

bool confirm_channel(std::string_view line, logic_an_input &input, std::string_view &error_message) {
    auto [ptr, er] = std::from_chars(line.begin(), line.end(), input.channel);
    if (ptr == line.end() && er == std::errc()) {
        if (input.channel > 28) {
            error_message = "CHANNEL: Your input isn't in range";
            input.channel = 29;
            return false;
        }
        error_message = "";
        return true;
    } else {
        error_message = "CHANNEL: Your input contains incorrect symbols";
        input.channel = 29;
    }
    return false;
}

bool confirm_freq(std::string_view line, logic_an_input &input, std::string_view &error_message) {
    input.hz = from_string_to_hz(line, error_message);
    if (input.hz != 0) {
        error_message = "";
        return true;
    }
    return false;
}

bool confirm_samples(std::string_view line, logic_an_input &input, std::string_view &error_message) {
    std::string temp_line{line};
    std::erase_if(temp_line, [](char x) { return (x == ','); });
    line = temp_line;
    auto [ptr, er] = std::from_chars(line.begin(), line.end(), input.samples);
    if (ptr == line.end() && er == std::errc()) {
        if (input.samples > 1'000'000) {
            error_message = "SAMPLES: Your input isn't in range";
            input.samples = 0;
            return false;
        }
        error_message = "";
        return true;
    } else {
        error_message = "SAMPLES: Your input contains incorrect symbols";
        input.samples = 0;
    }
    return false;
}

bool confirm_fst(std::string_view line, std::string_view &error_message) {
    if (line.size() > 255) {
        error_message = "OUTPUT: Too long name for file";
        return false;
    }
    std::string_view forbidden_symbols{"/*?<>|$"};

    for (auto v : line) {
        if (isspace(v) || forbidden_symbols.find(v) != std::string_view::npos) {
            error_message = "OUTPUT: Forbiddem symbol";
            return false;
        }
    }
    if (!line.ends_with(".fst")) {
        error_message = "OUTPUT: File name doesnt end with .fst";
        return false;
    }
    error_message = "";
    return true;
}

bool prepare_capture(std::string_view channel_placeholder, std::string_view frequency_placeholder,
                     std::string_view samples_placeholder, std::string_view output, std::string_view &error,
                     logic_an_input &input) {
    if (!confirm_channel(channel_placeholder, input, error)) {
        return false;
    } else if (!confirm_freq(frequency_placeholder, input, error)) {
        return false;
    } else if (!confirm_samples(samples_placeholder, input, error)) {
        return false;
    } else if (!confirm_fst(output, error)) {
        return false;
    }
    return true;
}