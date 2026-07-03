#include <format>
#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/ftxui.hpp>
#include <ftxui/screen/screen.hpp>
#include <string_view>
#include <utility>
#include "../../embedded/include/config.hpp"

ftxui::Element make_layout(
    const logic_an_input& input,
    const status_conf& status,
    const ftxui::Component& channel_input,
    const ftxui::Component& freq_input,
    const ftxui::Component& sample_input,
    const ftxui::Component& output_input,
    std::string_view error_input
);
