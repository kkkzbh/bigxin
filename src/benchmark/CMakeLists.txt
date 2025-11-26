# 批量注册账号工具
add_executable(register_accounts register_accounts.cpp)

target_link_libraries(register_accounts PRIVATE 
    Boost::asio
)

# 世界消息压测工具
add_executable(world_message_test world_message_test.cpp)

target_link_libraries(world_message_test PRIVATE 
    Boost::asio
)
