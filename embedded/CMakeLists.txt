add_executable(logic_analyzer
    src/main.cpp
    src/sampler.cpp
)

target_include_directories(logic_analyzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(logic_analyzer PRIVATE "cxx_std_${CMAKE_CXX_STANDARD}")

target_compile_options(logic_analyzer PRIVATE -Wall -Wextra -Wshadow -Wnull-dereference -pedantic)

target_compile_options(logic_analyzer PRIVATE $<$<CONFIG:Debug>:-Og -ggdb3> $<$<CONFIG:Release>:-O2>)

pico_enable_stdio_usb(logic_analyzer 1)
pico_enable_stdio_uart(logic_analyzer 0)


target_link_libraries(logic_analyzer  
                            pico_stdlib 
                            hardware_pwm
                            )

pico_add_extra_outputs(logic_analyzer)

find_program(PICOTOOL_EXECUTABLE picotool)

if(PICOTOOL_EXECUTABLE)
    add_custom_target(load
        COMMAND
            ${PICOTOOL_EXECUTABLE}
            load
            -f
            -x
            $<TARGET_FILE_DIR:logic_analyzer>/logic_analyzer.uf2

        COMMENT "Loading logic_analyzer.uf2 to Pico"
        USES_TERMINAL
        VERBATIM
    )

    add_dependencies(load logic_analyzer)
else()
    message(WARNING "picotool was not found: target 'load' will not be available")
endif()