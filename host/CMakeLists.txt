include(FetchContent)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v7.0.0
)
FetchContent_MakeAvailable(ftxui)
add_executable(logic_analyzer_host
    src/main.cpp
    src/elements_of_ui.cpp
    src/parser.cpp
)
target_include_directories(logic_analyzer_host
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(logic_analyzer_host
    PRIVATE
        cxx_std_20
)
target_compile_options(logic_analyzer_host
    PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wpedantic

        $<$<CONFIG:Debug>:-Og>
        $<$<CONFIG:Debug>:-ggdb3>
        $<$<CONFIG:Release>:-O2>
)

target_link_libraries(logic_analyzer_host
    PRIVATE
        ftxui::ftxui
)