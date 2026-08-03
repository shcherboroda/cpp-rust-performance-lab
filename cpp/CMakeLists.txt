cmake_minimum_required(VERSION 3.20)

project(cpp_sequential_sum LANGUAGES CXX)

add_executable(sequential_sum sequential_sum.cpp)
target_compile_features(sequential_sum PRIVATE cxx_std_20)

target_compile_options(sequential_sum PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
