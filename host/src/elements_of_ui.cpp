#include <cstdint>
#include <ftxui/component/app.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <string>
#include <string_view>
#include "elements_of_ui.hpp"

ftxui::Element static make_central(ftxui::Element el) {
    el = el | ftxui::center | ftxui::flex;
    return el;
}

ftxui::Element static sizing(ftxui::Element el, int8_t w, int8_t h) {
    el |= ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, w);
    el |= ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, h);
    el = el | ftxui::borderEmpty | ftxui::borderHeavy;
    return el;
}

ftxui::Element make_layout(const logic_an_input &input, const status_conf &status,
                           const ftxui::Component &channel_input, const ftxui::Component &freq_input,
                           const ftxui::Component &sample_input, const ftxui::Component &output_input,
                           std::string_view error_input) {
    auto left_content = ftxui::vbox({
        ftxui::hbox({
            ftxui::text("Channel(s) [0-28] (15 or 15+3→15-18): "),
            channel_input->Render(),
        }),
        ftxui::hbox({
            ftxui::text("Frequency [1 Hz-200 MHz]: "),
            freq_input->Render(),
        }),
        ftxui::hbox({
            ftxui::text("Samples [1-200,000]: "),
            sample_input->Render(),
        }),
    });
    left_content = make_central(left_content);

    auto left_box = ftxui::vbox({ftxui::center(ftxui::bold(ftxui::text("Logic Analyzer"))), ftxui::separatorDashed(),
                                 ftxui::filler(), left_content, ftxui::filler()});

    left_box = sizing(left_box, 60, 7);

    auto right_content = ftxui::vbox({
        ftxui::text(std::format("Device: {}", status.dev_con ? "connected" : "disconnected")),
        ftxui::text(std::format("Capture: {}", status.cap_status())),
        ftxui::hbox({
            ftxui::text("Output: "),
            output_input->Render(),
        }),
    });

    right_content = make_central(right_content);
    auto right_box = ftxui::vbox({ftxui::center(ftxui::bold(ftxui::text("Status"))), ftxui::separatorDashed(),
                                  ftxui::filler(), right_content, ftxui::filler()});
    right_box = sizing(right_box, 60, 7);

    auto high_panel = ftxui::hbox({left_box, ftxui::separatorEmpty(), right_box});
    auto control_content =
        ftxui::vbox({ftxui::text("Tab / Shift+Tab: change field"), ftxui::text("Enter: confirm field"),
                     ftxui::text("Ctrl+R: start recording"), ftxui::text("Ctrl+Q: quit")});
    control_content = make_central(control_content);
    auto control_box =
        ftxui::vbox({ftxui::center(ftxui::bold(ftxui::text("Controls"))), ftxui::separatorDashed(), control_content});
    control_box = sizing(control_box, 30, 1);

    auto error_content = ftxui::text(error_input);
    error_content = make_central(error_content);
    error_content |= ftxui::color(ftxui::Color::Red);
    auto error_box =
        ftxui::vbox({ftxui::center(ftxui::bold(ftxui::text("Errors"))), ftxui::separatorDashed(), error_content});
    error_box = sizing(error_box, 30, 1);

    auto combined_box = ftxui::hbox({control_box, error_box});
    combined_box |= ftxui::center;

    auto panel = ftxui::vbox({high_panel, combined_box});
    panel |= ftxui::center;
    return panel;
}
