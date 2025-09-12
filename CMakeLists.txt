cmake_minimum_required(VERSION 3.14)
project(CTS_Framework)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Google Test
find_package(GTest REQUIRED)

# 如果找不到GTest，尝试使用FetchContent
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50f33f9142fd2d6a2b9da0b52b2e0.zip
    )
    # 对于Windows：防止重写父项目的编译器/链接器设置
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# 创建CTS框架库
add_library(cts_framework INTERFACE)
target_include_directories(cts_framework INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cts_framework INTERFACE GTest::gtest GTest::gtest_main pthread)

# 创建CTS示例可执行文件
add_executable(cts_sample cts_sample.cpp)
target_link_libraries(cts_sample cts_framework)

# 启用测试
enable_testing()

# 添加测试
add_test(NAME cts_sample_test COMMAND cts_sample)

# 可选：添加安装规则
install(FILES cts_framework.h DESTINATION include)

# 输出有用的信息
message(STATUS "CTS Framework configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  GTest found: ${GTest_FOUND}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")

# 如果是Debug模式，添加调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(cts_framework INTERFACE -g -O0)
    message(STATUS "  Debug mode enabled")
endif()