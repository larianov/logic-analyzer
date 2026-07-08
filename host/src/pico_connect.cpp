#include "termios.h"
#include "pico_connect.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>

#include <thread>
#include <chrono>

#include <unistd.h>
#include <dirent.h>

bool rp_link::find_available_port() {
    auto *dir = opendir("/sys/class/tty");
    if (!dir) {
        fd_ = -1;
        return false;
    }
    struct dirent *entry;
    int fd{-1};
    std::string var_name{};
    std::string_view name_of_file;
    while ((entry = readdir(dir)) != nullptr) {
        fd = -1;
        name_of_file = entry->d_name;
        if (name_of_file.find("ACM") != std::string_view::npos || name_of_file.find("USB") != std::string_view::npos) {
            var_name = std::format("/sys/class/tty/{}/device/../idVendor", name_of_file);
            fd = open(var_name.c_str(), O_RDONLY);
            if (fd < 0)
                continue;
        } else
            continue;
        char buffer[5] = {};
        ssize_t bytes_read = read(fd, buffer, 4);
        close(fd);
        if (strncmp(buffer, "2e8a", 4) == 0) {
            var_name = std::format("/dev/{}", name_of_file);
            fd_ = open(var_name.c_str(), O_RDWR | O_NOCTTY);
            if (fd_ < 0)
                continue;
            struct termios tty{};
            if (tcgetattr(fd_, &tty) != 0) {
                close(fd_);
                fd_ = -1;
                continue;
            }
            cfmakeraw(&tty);
            tty.c_cc[VMIN] = 0;
            tty.c_cc[VTIME] = 5;
            if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
                close(fd_);
                fd_ = -1;
                continue;
            }
            tcflush(fd_, TCIFLUSH);
            uint8_t ping = 0x05;
            if (write(fd_, &ping, 1) != 1) {
                close(fd_);
                fd_ = -1;
                continue;
            }
            tcdrain(fd_);
            uint8_t pong{};
            bytes_read = read(fd_, &pong, 1);
            if (bytes_read > 0) {
                if (pong == 0x06) {
                    closedir(dir);
                    return true;
                } else {
                    close(fd_);
                    fd_ = -1;
                    continue;
                }
            } else {
                close(fd_);
                fd_ = -1;
                continue;
            }
            close(fd_);
            fd_ = -1;
            continue;
        }
    }
    closedir(dir);
    return false;
}

rp_link::rp_link() {
    find_available_port();
}

void rp_link::close_port() {
    if (fd_ == -1)
        return;
    close(fd_);
    fd_ = -1;
}

bool rp_link::send_handshake() {
    tcflush(fd_, TCIFLUSH);
    uint8_t ping = 0x05;
    if (write(fd_, &ping, 1) != 1) {
        close(fd_);
        fd_ = -1;
    }
    tcdrain(fd_);
    uint8_t pong{};
    ssize_t bytes_read = read(fd_, &pong, 1);
    if (bytes_read > 0) {
        if (pong == 0x06) {
            return true;
        } else {
            close(fd_);
            fd_ = -1;
        }
    }
    return false;
}

bool rp_link::is_alive() {
    if (fd_ < 0) {
        return false;
    }
    if (!send_handshake())
        return false;
    return true;
}
bool rp_link::capture_data(const logic_an_input &config) {
    buffer_.resize(config.samples);
    const std::uint8_t MAX_ATTEMPTS = 6;
    std::uint8_t attempts = 0;

    while (attempts < MAX_ATTEMPTS) {
        tcflush(fd_, TCIOFLUSH);

        if (write(fd_, &config, sizeof(config)) != sizeof(config)) {
            close_port();
            return false;
        }
        tcdrain(fd_);

        uint8_t ack{};
        if (read(fd_, &ack, 1) <= 0 || ack != 0x55) {
            attempts++;
            continue;
        }

        uint32_t sleep_ms = static_cast<uint32_t>(std::round((double(config.samples) / config.hz) * 1250.0));
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

        bool error_of_reading{};
        for (uint8_t i{}; i < 4; i++) {
            uint8_t sync{0x5A};
            if (write(fd_, &sync, 1) != 1) {
                close_port();
                return false;
            }

            sync = 0;
            int bytes_read = read(fd_, &sync, 1);

            if (bytes_read > 0 && sync == 0xA5) {
                break;
            }
            if (i == 3) {
                error_of_reading = true;
            }
        }

        if (error_of_reading) {
            attempts++;
            continue;
        }
        if (write(fd_, "\x5b", 1) != 1) {
            close_port();
            return false;
        }
        std::size_t total_received = 0;
        bool chunk_error = false;

        while (total_received < config.samples) {
            ssize_t chunk_read = read(fd_, buffer_.data() + total_received, config.samples - total_received);

            if (chunk_read > 0) {
                total_received += chunk_read;
            } else if (chunk_read == 0) {
                chunk_error = true;
                break;
            } else {
                close_port();
                return false;
            }
        }

        if (!chunk_error && total_received == config.samples) {
            finished_parsing_ = true;
            return true;
        }

        attempts++;
    }

    return false;
}

[[nodiscard]] std::optional<std::span<const uint8_t>> rp_link::getter() const {
    if (!finished_parsing_) {
        return std::nullopt;
    }
    return std::span<const uint8_t>{buffer_.data(), buffer_.size()};
}