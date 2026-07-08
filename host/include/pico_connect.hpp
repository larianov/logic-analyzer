#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "../../embedded/include/config.hpp"

class rp_link {
  private:
    int fd_ = -1;
    void close_port();
    bool send_handshake();
    std::vector<uint8_t> buffer_;
    bool send_req(const logic_an_input &config);
    bool finished_parsing_{};

  public:
    bool find_available_port();
    rp_link();
    ~rp_link() {
        close_port();
    }

    rp_link(const rp_link &) = delete;
    rp_link(rp_link &&) = default;
    rp_link operator=(const rp_link &) = delete;
    rp_link &operator=(rp_link &&) = default;
    bool is_alive();
    bool capture_data(const logic_an_input &config);
    [[nodiscard]] std::optional<std::span<const uint8_t>> getter() const;
};
