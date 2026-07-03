#include <cstddef>
#include <cstdio>
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

static ftxui::InputOption make_input_options()
{
    ftxui::InputOption options;
    options.multiline = false;
    options.transform = [](ftxui::InputState state) {
        return state.element;
    };

    return options;
}

static std::string format_samples(std::uint32_t value)
{
    return std::format(
        std::locale("en_US.UTF-8"),
        "{:L}",
        value
    );
}

int main(){
int selected_input = 0;
std::locale::global(std::locale("en_US.UTF-8"));
logic_an_input inpt{.channel = 3, .hz = 1'000'000, .samples = 10000};
status_conf stats{};
std::string channel_placeholder;
std::string frequency_placeholder;
std::string samples_placeholder;
std::string output{"capture.vcd"};
std::string_view error_message{};

auto channel_opt = make_input_options();
channel_opt.on_enter = [&]{
    if(confirm_channel(channel_placeholder, inpt, error_message)) selected_input = 1;
};
auto channel_inp = ftxui::Input(&channel_placeholder, "", channel_opt);

auto output_opt = make_input_options();
output_opt.on_enter = [&]{
    if(confirm_vcd(output, error_message)) selected_input = 3;
};
auto output_inp = ftxui::Input(&output, "", output_opt);

auto freq_opt = make_input_options();
freq_opt.on_enter = [&]{
    if(confirm_freq(frequency_placeholder, inpt, error_message)){
        selected_input = 2;
        frequency_placeholder = from_hz_to_string(inpt.hz);
    } 
};
auto freq_inp = ftxui::Input(&frequency_placeholder, "", freq_opt);

auto sample_opt = make_input_options();
sample_opt.on_enter = [&]{
    if(confirm_samples(samples_placeholder, inpt, error_message)){
        selected_input = 3;  
        samples_placeholder = format_samples(inpt.samples);
    } 
};
auto sample_inp = ftxui::Input(&samples_placeholder, "", sample_opt);


auto inputs = ftxui::Container::Vertical({
    channel_inp,
    freq_inp,
    sample_inp,
    output_inp
}, &selected_input);

auto renderer = ftxui::Renderer(inputs, [&] {
    return make_layout(inpt, stats, channel_inp, freq_inp, sample_inp, output_inp, error_message);
});
auto screen = ftxui::ScreenInteractive::TerminalOutput();
auto exit =screen.ExitLoopClosure();
auto app = ftxui::CatchEvent(renderer, [&](ftxui::Event ev){
    if(ev == ftxui::Event::CtrlQ){
        exit();
        return true;
    }
    else {
        return false;
    }
});
screen.Loop(app);
}