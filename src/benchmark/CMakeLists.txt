# Benchmark 模块 - 服务器压测工具
# 完全独立，不依赖其他模块的实现

add_executable(benchmark
    main.cpp
    benchmark_client.cpp
    account_manager.cpp
    benchmark_runner.cpp
)

target_include_directories(benchmark
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(benchmark
    PRIVATE
    Boost::asio
    nlohmann_json::nlohmann_json
)

# 设置 C++20 标准
target_compile_features(benchmark PRIVATE cxx_std_20)

# 添加协程支持（GCC 需要 -fcoroutines）
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(benchmark PRIVATE -fcoroutines)
endif()
