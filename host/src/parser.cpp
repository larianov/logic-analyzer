#include <algorithm>
#include <array>
#include <cmath>
#include "parser.hpp"
#include "elements_of_ui.hpp"
#include "iostream"
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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

static uint32_t hz_calib(uint32_t freq) {
    uint32_t best_sys_freq = 200'000'000;
    uint32_t min_diff = -1;
    uint32_t max_pio_div = 200'000'000 / freq;
    if (max_pio_div == 0)
        max_pio_div = 1;
    for (uint32_t pio_div = 1; pio_div <= max_pio_div; ++pio_div) {
        for (uint32_t p1 = 1; p1 <= 7; ++p1) {
            for (uint32_t p2 = 1; p2 <= p1; ++p2) {
                uint64_t target_vco = static_cast<uint64_t>(freq) * pio_div * p1 * p2;
                uint32_t fbdiv = (target_vco + 6'000'000) / 12'000'000;
                if (fbdiv < 16 || fbdiv > 320)
                    continue;
                uint64_t vco = 12'000'000ULL * fbdiv;
                if (vco < 400'000'000 || vco > 1'600'000'000)
                    continue;
                if (vco % (p1 * p2) != 0)
                    continue;
                uint32_t real_sys_freq = vco / (p1 * p2);
                uint32_t real_sample_hz = real_sys_freq / pio_div;
                uint32_t diff{};
                if (real_sample_hz > freq) {
                    diff = real_sample_hz - freq;
                } else {
                    diff = freq - real_sample_hz;
                }

                if (diff < min_diff) {
                    min_diff = diff;
                    best_sys_freq = real_sys_freq;
                    if (diff == 0) {
                        return best_sys_freq;
                    }
                }
            }
        }
    }
    return best_sys_freq;
}

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

static bool handling_char_conv(std::string_view line, uint8_t &digit, std::string_view &error_msg) {
    auto [ptr, er] = std::from_chars(line.data(), line.data() + line.size(), digit);
    if (ptr == line.data() + line.size() && er == std::errc()) {
        return true;
    } else {
        error_msg = "CHANNEL: Your input contains incorrect symbols";
        return false;
    }
}

bool confirm_channel(std::string_view line, logic_an_input &input, std::string_view &error_message) {
    if (line.empty()) {
        error_message = "CHANNEL: Your input is empty";
        return false;
    }
    std::size_t chin = line.find_first_not_of(' ');
    if (chin == std::string_view::npos) {
        error_message = "CHANNEL: Your input is empty";
        return false;
    }
    line.remove_prefix(chin);
    chin = line.find_last_not_of(' ');
    line.remove_suffix(line.size() - chin - 1);
    chin = line.find('+');
    auto temp_str = line;
    if (chin != std::string_view::npos) {
        temp_str = line.substr(0, chin);
    }
    if (!handling_char_conv(temp_str, input.channel, error_message))
        return false;
    if (input.channel > 28) {
        error_message = "CHANNEL: Out of range, range[0-28]";
        return false;
    }
    if (input.channel > 22 && input.channel < 26) {
        error_message =
            "CHANNEL: Logic anayzer doesn't support 23-25 gpio pins, if you want you can comment this in parser.cpp";
        return false;
    }
    if (chin == std::string_view::npos) {
        error_message = "";
        input.amm = 0;
        return true;
    }
    temp_str = line.substr(chin + 1);
    if (temp_str.empty()) {
        error_message = "CHANNEL: Missing amount after '+'";
        return false;
    }
    if (!handling_char_conv(temp_str, input.amm, error_message))
        return false;
    if (input.amm > 7) {
        error_message = "CHANNEL: Channel Range exceeds limits of channels";
        return false;
    }
    if ((input.channel + input.amm) > 28) {
        error_message = "CHANNEL: Channel Range overflowed 28 channels";
        return false;
    }
    for (uint8_t i{}; i <= input.amm; i++) {
        if (auto temp = input.channel + i; temp >= 23 && temp <= 25) {
            error_message = "CHANNEL: Logic anayzer doesn't support 23-25 gpio pins, if you want you can comment this "
                            "in parser.cpp";
            return false;
        }
    }
    error_message = "";
    return true;
}

bool confirm_freq(std::string_view line, logic_an_input &input, std::string_view &error_message) {
    input.hz = from_string_to_hz(line, error_message);
    if (input.hz != 0) {
        if (200'000'000 % input.hz != 0 && input.hz >= 2'000'000) {
            auto sys_clk = hz_calib(input.hz);
            auto div = sys_clk / input.hz;
            if (div == 0)
                div = 1;
            input.hz = sys_clk / div;
        }
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
        if (input.samples > 200'000) {
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
    if (error == "WARNING: Capture time exceeds 10s.\nPress Ctrl+R again to confirm.") {
        error = "";
        return true;
    }
    if (!confirm_channel(channel_placeholder, input, error)) {
        return false;
    } else if (!confirm_freq(frequency_placeholder, input, error)) {
        return false;
    } else if (!confirm_samples(samples_placeholder, input, error)) {
        return false;
    } else if (!confirm_fst(output, error)) {
        return false;
    }
    if ((((1.0F / static_cast<float>(input.hz)) * static_cast<float>(input.samples)) > 10)) {
        error = "WARNING: Capture time exceeds 10s.\nPress Ctrl+R again to confirm.";
        return false;
    }
    return true;
}

bool confirm_instance(std::vector<t_decoders>&ver, std::string& ch1placeholder, std::string& ch2placeholder, std::string& ch3placeholder, uint8_t used_channels, logic_an_input& input, std::string_view& error_for_dec, c_decoders ix){
    t_decoders to_ret{.code = c_decoders::ERROR, .first_ch = 29, .second_ch = 29, .third_ch = std::nullopt};
    auto rem_chan = input.amm+1 - used_channels;
    if ((ix == c_decoders::I2C || ix == c_decoders::UART) && rem_chan < 2 ) {
        error_for_dec = "For protocols I2C or UART you need minimum 2 free channels.";
        return false;
    }
    else if (ix == c_decoders::SPI && rem_chan < 3) {
        error_for_dec = "For protocols SPI you need minimum 3 free channels.";
        return false;
    }
    
    to_ret.code = ix;
    if(handling_char_conv(ch1placeholder, to_ret.first_ch, error_for_dec)) return false;
    
    if(handling_char_conv(ch2placeholder, to_ret.second_ch, error_for_dec)) return false;
    
    if (ix == c_decoders::SPI) {
        if(handling_char_conv(ch3placeholder, to_ret.third_ch.value(), error_for_dec)) return false;
    }
    if (to_ret.first_ch == 29 || to_ret.second_ch == 29 || (ix == c_decoders::SPI && !to_ret.third_ch.has_value())) {
        error_for_dec = "All fields should be initialized.";
        return false;
    }
    for(auto x : ver){
        if (to_ret.first_ch == x.first_ch || to_ret.first_ch == x.second_ch || (x.third_ch.has_value() && x.third_ch.value() == to_ret.first_ch)) {
            error_for_dec = "Your input for first channel is already used.\n Please use another channel.";
            return false;
        }
        if (to_ret.second_ch == x.first_ch || to_ret.second_ch == x.second_ch || (x.third_ch.has_value() && x.third_ch.value() == to_ret.second_ch)) {
            error_for_dec = "Your input for second channel is already used.\n Please use another channel.";
            return false;
        }
        if (to_ret.third_ch.has_value() && (to_ret.third_ch.value() == x.first_ch || to_ret.third_ch.value() == x.second_ch || (x.third_ch.has_value() && x.third_ch.value() == to_ret.third_ch.value()))) {
            error_for_dec = "Your input for third channel is already used.\n Please use another channel.";
            return false;
        }
    }
    if (to_ret.first_ch < input.channel || to_ret.first_ch > input.channel+input.amm ) {
        error_for_dec = "Your input out of range of your choosen range of channels";
        return false;
    }
    if (to_ret.first_ch == to_ret.second_ch || (to_ret.third_ch.has_value() && (to_ret.second_ch == to_ret.third_ch.value() || to_ret.first_ch == to_ret.third_ch.value()))) {
        error_for_dec = "Your channels are repeating. All channels should be different";
        return false;
    }
    ver.emplace_back(to_ret);
    return true;
}