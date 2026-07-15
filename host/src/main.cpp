#include <fcntl.h>
#include <thread>
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
    std::string_view error_message{};

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
        } else {
            return false;
        }
    });
    screen.Loop(app);
}