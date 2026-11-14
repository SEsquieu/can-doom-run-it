# SPDX-License-Identifier: GPL-3.0-or-later
add_library(cdr_io STATIC control_bus.cpp udp_output.cpp)
target_compile_features(cdr_io PRIVATE cxx_std_17)
if(WIN32)
    target_link_libraries(cdr_io PUBLIC ws2_32)
endif()

if(BUILD_TESTING)
    add_executable(cdr_io_tests "${CMAKE_SOURCE_DIR}/tests/control_bus_test.cpp")
    target_include_directories(cdr_io_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(cdr_io_tests PRIVATE cdr_io)
    add_test(NAME cdr.native_control COMMAND cdr_io_tests)
    set_tests_properties(cdr.native_control PROPERTIES TIMEOUT 10)
endif()
