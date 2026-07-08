#include <hardware/timer.h>
#include "sampler.hpp"
#include "config.hpp"
#include <cstdint>
#include <hardware/gpio.h>
#include <optional>
#include <pico/platform/common.h>
#include <pico/time.h>
#include <span>

void Sampler::init(const logic_an_input inpt) {
    inpt_for_sampling = inpt;
    tact_time = 1'000'000 / inpt_for_sampling.hz;
    gpio_init(inpt_for_sampling.channel);
    gpio_set_dir(inpt_for_sampling.channel, GPIO_IN);
}

[[nodiscard]] std::optional<std::span<const std::uint8_t>> Sampler::samples() const {
    if (still_measuring) {
        return std::nullopt;
    }
    return std::span<const uint8_t>{samples_.data(), inpt_for_sampling.samples};
}

void Sampler::start_sampling() {
    auto next = get_absolute_time();
    const auto start_time = next;
    for (uint32_t i{}; i < inpt_for_sampling.samples; i++) {
        next = delayed_by_us(next, tact_time);
        busy_wait_until(next);
        samples_[i] = gpio_get(inpt_for_sampling.channel);
    }
    still_measuring = false;
}