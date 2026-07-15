#include "config.hpp"
#include <array>
#include <cstdint>
#include <expected>
#include <hardware/structs/pio.h>
#include <optional>
#include <span>
#include <utility>

struct pio_t {
    pio_hw_t *pio;
    uint8_t sm;
    int offset;
};

class Sampler {
  public:
    Sampler() = default;
    void init(const logic_an_input inpt);
    void start_sampling();
    [[nodiscard]] std::optional<std::span<const std::uint8_t>> samples() const;

  private:
    bool still_measuring{true};
    static constexpr std::uint32_t max_samples = 200'000;
    logic_an_input inpt_for_sampling{};
    uint8_t ammount_of_channels{};
    alignas(4) static inline volatile uint8_t samples_[max_samples];
    bool slow_mode{};
    pio_t pio{};
};
