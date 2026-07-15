#include <algorithm>
#include <cmath>
#include <cstdio>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/pio.h>
#include <hardware/structs/clocks.h>
#include <hardware/structs/pio.h>
#include <hardware/timer.h>
#include "sampler.hpp"
#include "config.hpp"
#include <cstdint>
#include <hardware/gpio.h>
#include <optional>
#include <pico/platform/common.h>
#include <pico/stdio.h>
#include <pico/stdio_usb.h>
#include <pico/time.h>
#include <pico/types.h>
#include <span>
#include <sys/stat.h>
#include <sys/unistd.h>
#include "read_pin.pio.h"

pio_t static initalize_sm(uint8_t pin, uint32_t hz, bool slow_mode) {
    pio_hw_t *pio = pio0;
    uint8_t sm = pio_claim_unused_sm(pio, true);
    auto offset = pio_add_program(pio, &logic_analyzer_program);
    auto config = logic_analyzer_program_get_default_config(offset);
    for (uint8_t i{0}; i < 8; i++) {
        pio_gpio_init(pio, pin+i);
    }
    sm_config_set_in_pins(&config, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 8, false);
    sm_config_set_in_shift(&config, false, true, 8);
    float div = float(clock_get_hz(clk_sys)) / float(hz);
    if (slow_mode) div = 1.0F;
    sm_config_set_clkdiv(&config, div);
    pio_sm_init(pio, sm, offset+static_cast<int>(slow_mode), &config);
    if (slow_mode) {
        pio_sm_put_blocking(pio, sm, ((clock_get_hz(clk_sys)/ hz) -4));
    }
    pio_t to_ret{.pio = pio, .sm = sm, .offset = offset};    
    return to_ret;
}

uint32_t calculate_best_pll(uint32_t target_sample_hz) {
    uint32_t best_sys_freq = 200'000'000;
    uint32_t min_diff = -1;
    uint32_t max_pio_div = 200'000'000 / target_sample_hz;
    if (max_pio_div == 0)
        max_pio_div = 1;
    for (uint32_t pio_div = 1; pio_div <= max_pio_div; ++pio_div) {
        for (uint32_t p1 = 1; p1 <= 7; ++p1) {
            for (uint32_t p2 = 1; p2 <= 7; ++p2) {
                uint64_t target_vco = static_cast<uint64_t>(target_sample_hz) * pio_div * p1 * p2;
                uint32_t fbdiv = (target_vco + 6'000'000) / 12'000'000;
                if (fbdiv < 16 || fbdiv > 320)
                    continue;
                uint64_t vco = 12'000'000ULL * fbdiv;
                if (vco < 400'000'000 || vco > 1'600'000'000)
                    continue;
                uint32_t real_sys_freq = vco / (p1 * p2);
                uint32_t real_sample_hz = real_sys_freq / pio_div;
                uint32_t diff{};
                if (real_sample_hz > target_sample_hz) {
                    diff = real_sample_hz - target_sample_hz;
                } else {
                    diff = target_sample_hz - real_sample_hz;
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

void Sampler::init(const logic_an_input inpt) {
    inpt_for_sampling = inpt;
    ammount_of_channels = inpt.amm;

    if ((clock_get_hz(clk_sys) / 65536) > inpt_for_sampling.hz) {
        slow_mode = true;
    } else if (clock_get_hz(clk_sys) % inpt.hz != 0 && inpt_for_sampling.hz >= 2'000'000) {
        uint32_t best_sys_clk = calculate_best_pll(inpt_for_sampling.hz);
        set_sys_clock_hz(best_sys_clk, true);
        uint32_t pio_div = best_sys_clk / inpt.hz;
        if (pio_div == 0)
            pio_div = 1;
        inpt_for_sampling.hz = best_sys_clk / pio_div;
    }
    pio = initalize_sm(inpt.channel, inpt.hz, slow_mode);
}

void Sampler::start_sampling() {    
    uint8_t dma_chan = dma_claim_unused_channel(true);
    auto conf = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&conf, DMA_SIZE_8);
    channel_config_set_read_increment(&conf, false);
    channel_config_set_write_increment(&conf, true);
    channel_config_set_dreq(&conf, pio_get_dreq(pio.pio, pio.sm, false));  
    dma_channel_configure(dma_chan, &conf, &samples_, &pio.pio->rxf[pio.sm], inpt_for_sampling.samples, true);
    pio_sm_set_enabled(pio.pio, pio.sm, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    dma_channel_unclaim(dma_chan);
    pio_sm_set_enabled(pio.pio, pio.sm, false); 
    still_measuring = false;
}

[[nodiscard]] std::optional<std::span<const std::uint8_t>> Sampler::samples() const {
    if (still_measuring) {
        return std::nullopt;
    }
    return std::span<const uint8_t>{const_cast<const uint8_t*>(samples_), inpt_for_sampling.samples};
}