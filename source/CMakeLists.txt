cmake_minimum_required (VERSION 3.20)
project(xwrflasher)
add_executable(xwrflasher spi.cpp gpio.cpp main.cpp crc.cpp)
include_directories(BEFORE ${CMAKE_PREFIX_PATH}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_link_directories(xwrflasher BEFORE PUBLIC ${CMAKE_PREFIX_PATH}/lib)
target_link_libraries(xwrflasher gpiod pthread rt)
set_property(TARGET xwrflasher PROPERTY CXX_STANDARD 17)
install(TARGETS xwrflasher DESTINATION bin)

