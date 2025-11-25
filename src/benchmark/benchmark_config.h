#pragma once

/**
 * @file benchmark_config.h
 * @brief 压力测试配置参数
 * 
 * 修改此文件后重新编译即可生效
 */

#include <string>
#include <cstddef>
#include <cstdint>

namespace benchmark {

// ==================== 服务器配置 ====================
inline const std::string SERVER_HOST = "127.0.0.1";
inline constexpr uint16_t SERVER_PORT = 5555;

// ==================== 账号配置 ====================
/// 账号前缀，用于区分不同批次的测试
/// 修改此值可以避免与之前的测试数据冲突
inline const std::string ACCOUNT_PREFIX = "t11";

/// 统一密码
inline const std::string PASSWORD = "1";

// ==================== 注册测试配置 ====================
/// 要注册的账号数量
inline constexpr size_t NUM_ACCOUNTS = 1000;

/// 最大并发连接数
inline constexpr size_t REGISTER_MAX_CONCURRENT = 1000;

// ==================== 消息测试配置 ====================
/// 并发客户端数
inline constexpr size_t NUM_CLIENTS = 500;

/// 每个客户端发送的消息数
inline constexpr size_t MESSAGES_PER_CLIENT = 100;

/// 最大并发连接数
inline constexpr size_t MESSAGE_MAX_CONCURRENT = 500;

// ==================== 群聊与观测账号配置 ====================
/// 群大小（1000 人按 100 分组 => 10 个群）
inline constexpr size_t GROUP_SIZE = 100;
/// 额外注册的观测账号（加入全部群，用于登录观察）
inline const std::string OBSERVER_ACCOUNT = "ob5";

} // namespace benchmark
