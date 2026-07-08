include(FetchContent)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
    GIT_TAG v7.0.0
)
FetchContent_MakeAvailable(ftxui)

enable_language(C)
FetchContent_Declare(
    fstapi_src
    GIT_REPOSITORY https://github.com/gtkwave/gtkwave
    GIT_TAG v3.3.116
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(fstapi_src)

set(FSTAPI_DIR ${fstapi_src_SOURCE_DIR}/gtkwave3-gtk3/src/helpers/fst)

find_package(ZLIB REQUIRED)

add_library(fstapi STATIC
    ${FSTAPI_DIR}/fstapi.c
    ${FSTAPI_DIR}/fastlz.c
    ${FSTAPI_DIR}/lz4.c
)
target_include_directories(fstapi PUBLIC ${FSTAPI_DIR})
target_link_libraries(fstapi PUBLIC ZLIB::ZLIB)
target_compile_definitions(fstapi PRIVATE FST_CONFIG_INCLUDE="fstapi.h")


add_executable(logic_analyzer_host
    src/main.cpp
    src/elements_of_ui.cpp
    src/pico_connect.cpp
    src/fst_parser.cpp
    src/parser.cpp
)
target_include_directories(logic_analyzer_host
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(logic_analyzer_host PRIVATE cxx_std_20)
target_compile_options(logic_analyzer_host
    PRIVATE
        -Wall -Wextra -Wshadow -Wpedantic
        $<$<CONFIG:Debug>:-Og>
        $<$<CONFIG:Debug>:-ggdb3>
        $<$<CONFIG:Release>:-O2>
)
target_link_libraries(logic_analyzer_host
    PRIVATE
        ftxui::ftxui
        fstapi
)