#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <ftxui/screen/color.hpp>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <format>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/ftxui.hpp>
#include <ftxui/screen/screen.hpp>
#include <string>
#include <string_view>
#include <utility>
#include "../../embedded/include/config.hpp"
#include <locale>
#include <vector>
#include "elements_of_ui.hpp"
#include "parser.hpp"
#include "pico_connect.hpp"
#include "fst_parser.hpp"

static ftxui::InputOption make_input_options() {
    ftxui::InputOption options;
    options.multiline = false;
    options.transform = [](ftxui::InputState state) { return state.element; };

    return options;
}

static std::string format_samples(std::uint32_t value) {
    return std::format(std::locale("en_US.UTF-8"), "{:L}", value);
}

int main() {
    status_conf stats{};
    rp_link rpl{};
    if (rpl.is_alive()) {
        stats.dev_con = true;
    }
    bool still_capturing{};
    std::jthread capture_thread;
    int selected_input = 0;
    std::locale::global(std::locale("en_US.UTF-8"));
    logic_an_input inpt{.msg = 6, .amm = 0, .channel = 15, .samples = 10000, .hz = 1'000'000};
    std::string channel_placeholder{};
    std::string frequency_placeholder{};
    std::string samples_placeholder{};
    std::string output{"capture.fst"};
    bool modal_is_open{};
    std::string_view error_message{};
    std::string_view error_for_dec{};
    std::vector<std::string_view> decoders_names{"I2C", "SPI", "UART"};
    uint8_t used_channels{};
    int selected_decoder{};
    std::string dec1ch_placeholder{};
    auto de1ch_opt = make_input_options();
    auto de1ch_inputu = ftxui::Input(&dec1ch_placeholder, "", de1ch_opt);

    std::string dec2ch_placeholder{};
    auto de2ch_opt = make_input_options();
    auto de2ch_inputu = ftxui::Input(&dec2ch_placeholder, "", de2ch_opt);

    std::string dec3ch_placeholder{};
    auto de3ch_opt = make_input_options();
    auto de3ch_inputu = ftxui::Input(&dec3ch_placeholder, "", de3ch_opt);

    std::vector<t_decoders> decoders;

    auto toggle = ftxui::Toggle(decoders_names, &selected_decoder);
    bool show_third{};
    auto maybe_third = ftxui::Maybe(de3ch_inputu, &show_third);
    auto last_decoder = selected_decoder;
    auto vertical = ftxui::Container::Vertical({toggle, de1ch_inputu, de2ch_inputu, maybe_third});
    auto renderer_vert = ftxui::Renderer(vertical, [&]() {
        if (last_decoder != selected_decoder) error_for_dec = "";
        show_third = decoders_names[selected_decoder] == "SPI";
        auto remaining = (1 + inpt.amm) - used_channels;
        auto i2c_box = ftxui::vbox(ftxui::hbox(ftxui::text("SDA: "), de1ch_inputu->Render()),
                                   ftxui::hbox(ftxui::text("SCL: "), de2ch_inputu->Render()));

        auto uart_box = ftxui::vbox(ftxui::hbox(ftxui::text("RX: "), de1ch_inputu->Render()),
                                    ftxui::hbox(ftxui::text("TX: "), de2ch_inputu->Render()));

        auto spi_box = ftxui::vbox(ftxui::hbox(ftxui::text("MOSI: "), de1ch_inputu->Render()),
                                   ftxui::hbox(ftxui::text("MISO: "), de2ch_inputu->Render()),
                                   ftxui::hbox(ftxui::text("SCK: "), maybe_third->Render()));

        ftxui::Element fields_box;
        if (decoders_names[selected_decoder] == "I2C")
            fields_box = i2c_box;
        else if (decoders_names[selected_decoder] == "UART")
            fields_box = uart_box;
        else
            fields_box = spi_box;

        auto box = ftxui::vbox(
            {ftxui::text(" Select Decoder ") | ftxui::bold | ftxui::center, ftxui::separator(),
             ftxui::hbox(ftxui::text(std::format("Remaining channels: {}", remaining)) | ftxui::center, ftxui::filler(),
                         ftxui::separator(), ftxui::filler(), ftxui::text("\'q\' for quitting") | ftxui::center,
                         ftxui::filler()),
             ftxui::separator(), toggle->Render(), fields_box, ftxui::separator(),
             ftxui::hbox(ftxui::text("Errors: "), ftxui::text(error_for_dec) | ftxui::color(ftxui::Color::Red))});
        box |= ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 55);
        box |= ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 8);
        box = box | ftxui::border | ftxui::clear_under | ftxui::center;
        last_decoder = selected_decoder;
        return box;
    });

    auto app_vert = ftxui::CatchEvent(renderer_vert, [&](ftxui::Event ev){
        if (ev == ftxui::Event::CtrlS) {
            auto ix = c_decoders::ERROR;
            if (decoders_names[selected_decoder] == "I2C")
                ix = c_decoders::I2C;
            if (decoders_names[selected_decoder] == "UART")
                ix = c_decoders::UART;

            if (decoders_names[selected_decoder] == "SPI")
                ix = c_decoders::SPI;

            if (confirm_instance(decoders, dec1ch_placeholder, dec2ch_placeholder, dec3ch_placeholder, used_channels, inpt, error_for_dec, ix)) {
                if (ix == c_decoders::SPI) used_channels +=3;
                else used_channels+=2;
                modal_is_open = false;
                return true;
            }
            return true;
        }
        if (ev == ftxui::Event::q) {
            error_for_dec = "";
            modal_is_open = false;
            return true;
        }

        return false;
    });

    auto channel_opt = make_input_options();
    channel_opt.on_enter = [&] {
        if (confirm_channel(channel_placeholder, inpt, error_message))
            selected_input = 1;
    };
    auto channel_inp = ftxui::Input(&channel_placeholder, "", channel_opt);

    auto output_opt = make_input_options();
    output_opt.on_enter = [&] {
        if (confirm_fst(output, error_message))
            selected_input = 3;
    };
    auto output_inp = ftxui::Input(&output, "", output_opt);

    auto freq_opt = make_input_options();
    freq_opt.on_enter = [&] {
        if (confirm_freq(frequency_placeholder, inpt, error_message)) {
            selected_input = 2;
            frequency_placeholder = from_hz_to_string(inpt.hz);
        }
    };
    auto freq_inp = ftxui::Input(&frequency_placeholder, "", freq_opt);

    auto sample_opt = make_input_options();
    sample_opt.on_enter = [&] {
        if (confirm_samples(samples_placeholder, inpt, error_message)) {
            selected_input = 3;
            samples_placeholder = format_samples(inpt.samples);
        }
    };
    auto sample_inp = ftxui::Input(&samples_placeholder, "", sample_opt);

    auto inputs = ftxui::Container::Vertical({channel_inp, freq_inp, sample_inp, output_inp}, &selected_input);

    auto renderer = ftxui::Renderer(
        inputs, [&] { return make_layout(inpt, stats, channel_inp, freq_inp, sample_inp, output_inp, error_message); });
    auto screen = ftxui::ScreenInteractive::TerminalOutput();
    auto exit = screen.ExitLoopClosure();

    auto app = ftxui::CatchEvent(renderer, [&](ftxui::Event ev) {
        if (ev == ftxui::Event::Special("Capture success")) {
            stats.cap_status_ = capturing::DONE;
            error_message = "";
            still_capturing = false;
            auto result_of_parsing = rpl.getter();
            if (result_of_parsing)
                fst_parse(result_of_parsing.value(), inpt, output);
            return true;
        }
        if (ev == ftxui::Event::Special("Capture unsuccess")) {
            stats.cap_status_ = capturing::IDLE;
            error_message = "CAPTURING: Capturing was unsuccessful, remove device, and try again.";
            still_capturing = false;
            stats.dev_con = false;
            return true;
        }

        if (still_capturing) {
            return true;
        }
        if (ev == ftxui::Event::CtrlQ) {
            exit();
            return true;
        } else if (ev == ftxui::Event::CtrlR) {
            if (prepare_capture(channel_placeholder, frequency_placeholder, samples_placeholder, output, error_message,
                                inpt)) {
                if (rpl.is_alive() || rpl.find_available_port()) {
                    stats.cap_status_ = capturing::CAPTURING;
                    stats.dev_con = true;
                    still_capturing = true;
                    capture_thread = std::jthread([&rpl, &screen, inpt] {
                        const bool suc = rpl.capture_data(inpt);
                        if (suc)
                            screen.PostEvent(ftxui::Event::Special("Capture success"));
                        else
                            screen.PostEvent(ftxui::Event::Special("Capture unsuccess"));
                    });
                } else {
                    stats.dev_con = false;
                    error_message = "DEVICE: Device with needed code isn't connected";
                }
            }
            return true;
        } else if (ev == ftxui::Event::CtrlA) {
            if (!confirm_channel(channel_placeholder, inpt, error_message))
                return false;
            modal_is_open = true;
            return true;
        } else if (ev == ftxui::Event::CtrlX) {
            used_channels = 0;
            decoders.clear();
            return true;
        } else {
            return false;
        }
    });
    auto modal = ftxui::Modal(app, app_vert, &modal_is_open);
    screen.Loop(modal);
}
