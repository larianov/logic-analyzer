#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <format>
#include <type_traits>
#include <cstddef>

struct alignas(4) logic_an_input {
    uint8_t msg;
    uint8_t channel;
    uint32_t hz;
    uint32_t samples;
};

inline std::string from_hz_to_string(unsigned int hz) {
    if (hz >= 1'000'000) {
        return std::format("{:.2f} MHz", hz / 1'000'000.0);
    } else if (hz >= 1'000) {
        return std::format("{:.2f} kHz", hz / 1'000.0);
    } else {
        return std::format("{} Hz", hz);
    }
}

enum class capturing : std::uint8_t {
    IDLE,
    READY,
    CAPTURING,
    DONE
};

struct status_conf {
    bool dev_con;
    capturing cap_status_;
    std::string_view name_of_file = "capture.vcd";
    [[nodiscard]] std::string_view cap_status() const {
        switch (cap_status_) {
        case capturing::IDLE:
            return "IDLE";
        case capturing::READY:
            return "READY";
        case capturing::CAPTURING:
            return "CAPTURING";
        case capturing::DONE:
            return "DONE";
        default:
            return "IDLE";
        }
    }
};

namespace ping {
static constexpr uint8_t PING_HANDSHAKE = 0x05;
static constexpr uint8_t PONG_HANDSHAKE = 0x06;
static constexpr uint8_t SIGN_OF_STRUCT = 0x06;
static constexpr uint8_t READY_CONFIG = 0x55;
static constexpr uint8_t PING_SAMPLING = 0x5A;
static constexpr uint8_t PONG_SAMPLING = 0xA5;
static constexpr uint8_t PING_SEND_SAMPLING = 0x5B;
} // namespace ping
