/**
 * @file world_message_test.cpp
 * @brief 世界频道消息压测工具
 * 
 * 功能:
 * 1. 批量登录已注册的账号
 * 2. 使用 LOGIN_RESP 返回的 worldConversationId 作为目标会话
 * 3. 向世界频道发送消息
 * 4. 统计 TPS、延迟、成功率等指标
 * 
 * 前置条件:
 * 先运行 register_accounts 注册测试账号
 */

#include "benchmark_config.h"
#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <print>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include <random>
#include <optional>
#include <thread>

namespace asio = boost::asio;
using namespace asio::experimental::awaitable_operators;
using namespace std::chrono_literals;
using namespace benchmark;
using nlohmann::json;

// ==================== 统计数据 ====================
struct Statistics {
    std::atomic<size_t> connections_success{0};
    std::atomic<size_t> connections_failed{0};
    std::atomic<size_t> logins_success{0};
    std::atomic<size_t> logins_failed{0};
    std::atomic<size_t> messages_sent{0};
    std::atomic<size_t> messages_acked{0};
    std::atomic<size_t> messages_received{0};
    
    std::atomic<uint64_t> total_latency_us{0};
    std::atomic<size_t> latency_samples{0};
    
    std::atomic<size_t> active_clients{0};
    
    std::chrono::steady_clock::time_point start_time;
    
    void print_progress() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto duration_sec = std::chrono::duration<double>(duration).count();
        auto tps = duration_sec > 0 ? messages_sent.load() / duration_sec : 0;
        
        std::println("[{:.1f}s] 活跃: {} | 发送: {} | 确认: {} | 接收: {} | TPS: {:.0f}",
            duration_sec, active_clients.load(),
            messages_sent.load(), messages_acked.load(), 
            messages_received.load(), tps);
    }
    
    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto duration_sec = std::chrono::duration<double>(duration).count();
        
        std::println("\n========== 世界消息测试结果 ==========");
        std::println("运行时间: {:.2f} 秒", duration_sec);
        std::println("");
        std::println("连接统计:");
        std::println("  成功: {} / 失败: {}", 
            connections_success.load(), connections_failed.load());
        std::println("");
        std::println("登录统计:");
        std::println("  成功: {} / 失败: {}", 
            logins_success.load(), logins_failed.load());
        std::println("");
        std::println("消息统计:");
        std::println("  发送: {} 条", messages_sent.load());
        std::println("  确认 (ACK): {} 条", messages_acked.load());
        std::println("  接收 (PUSH): {} 条", messages_received.load());
        std::println("  发送 TPS: {:.2f} 条/秒", messages_sent.load() / duration_sec);
        
        if (latency_samples.load() > 0) {
            auto avg_latency = total_latency_us.load() / latency_samples.load();
            std::println("");
            std::println("延迟统计:");
            std::println("  平均延迟: {} μs ({:.2f} ms)", 
                avg_latency, avg_latency / 1000.0);
        }
        
        // 计算消息确认率
        if (messages_sent.load() > 0) {
            auto ack_rate = 100.0 * messages_acked.load() / messages_sent.load();
            std::println("");
            std::println("可靠性:");
            std::println("  ACK 确认率: {:.2f}%", ack_rate);
        }
        
        std::println("=======================================\n");
    }
};

// 全局统计对象
Statistics g_stats;

// ==================== 协议工具函数 ====================

struct GroupConfig {
    size_t group_size{GROUP_SIZE};
    std::unordered_map<size_t, std::string> conv_by_group;
    std::string observer_account;
};

std::optional<GroupConfig> load_group_config() {
    std::ifstream ifs("benchmark_groups.json");
    if (!ifs.is_open()) return std::nullopt;
    try {
        json j;
        ifs >> j;
        GroupConfig cfg;
        if (j.contains("groupSize")) cfg.group_size = j["groupSize"].get<size_t>();
        if (j.contains("observerAccount")) cfg.observer_account = j["observerAccount"].get<std::string>();
        if (j.contains("groups") && j["groups"].is_array()) {
            for (auto const& item : j["groups"]) {
                if (item.contains("index") && item.contains("conversationId")) {
                    cfg.conv_by_group[item["index"].get<size_t>()] = item["conversationId"].get<std::string>();
                }
            }
        }
        return cfg;
    } catch (...) {
        return std::nullopt;
    }
}

/// 构建账号名称
std::string build_account_name(int user_id) {
    return std::format("{}user_{}", ACCOUNT_PREFIX, user_id);
}

/// 构建登录消息
std::string build_login_message(int user_id) {
    auto account = build_account_name(user_id);
    return std::format(
        "LOGIN:{{\"account\":\"{}\",\"password\":\"{}\"}}\n", 
        account, PASSWORD);
}

/// 构建消息发送 (使用世界频道 conversationId)
std::string build_client_msg_id(const std::string& sender_id, int msg_seq) {
    return std::format("world-{}-{}", sender_id, msg_seq);
}

std::string build_send_message(const std::string& conversation_id, 
                                const std::string& sender_id, 
                                int msg_seq,
                                std::string const& client_msg_id) {
    return std::format(
        "SEND_MSG:{{\"conversationId\":\"{}\",\"senderId\":\"{}\","
        "\"clientMsgId\":\"{}\",\"msgType\":\"TEXT\","
        "\"content\":\"世界消息 #{}\"}}\n",
        conversation_id, sender_id, client_msg_id, msg_seq);
}

/// 解析登录响应
struct LoginResponse {
    bool ok{false};
    std::string user_id;
    std::string display_name;
    std::string world_conversation_id;
    std::string error_msg;
    
    static LoginResponse parse(const std::string& line) {
        LoginResponse resp;
        
        if (line.find("LOGIN_RESP:") != 0) {
            resp.error_msg = "非 LOGIN_RESP 响应";
            return resp;
        }
        
        if (line.find("\"ok\":true") != std::string::npos) {
            resp.ok = true;
            
            // 提取 userId
            auto extract = [&line](const std::string& key) -> std::string {
                auto pos = line.find("\"" + key + "\":\"");
                if (pos == std::string::npos) return "";
                auto start = pos + key.length() + 4;
                auto end = line.find("\"", start);
                if (end == std::string::npos) return "";
                return line.substr(start, end - start);
            };
            
            resp.user_id = extract("userId");
            resp.display_name = extract("displayName");
            resp.world_conversation_id = extract("worldConversationId");
        } else {
            auto pos = line.find("\"errorMsg\":\"");
            if (pos != std::string::npos) {
                auto start = pos + 12;
                auto end = line.find("\"", start);
                if (end != std::string::npos) {
                    resp.error_msg = line.substr(start, end - start);
                }
            }
        }
        
        return resp;
    }
};

/// 判断响应类型
bool is_send_ack(const std::string& line) {
    return line.find("SEND_ACK:") == 0;
}

bool is_msg_push(const std::string& line) {
    return line.find("MSG_PUSH:") == 0;
}

// ==================== 测试客户端 ====================

class WorldTestClient : public std::enable_shared_from_this<WorldTestClient> {
public:
    WorldTestClient(asio::any_io_executor exec, int client_id, const GroupConfig* cfg)
        : socket_(exec)
        , client_id_(client_id)
        , group_cfg_(cfg)
    {}
    
    /// 执行完整的测试流程
    asio::awaitable<void> run() {
        // 保持对象生命周期直到 run 结束
        auto self = shared_from_this();
        g_stats.active_clients++;
        
        try {
            // 1. 连接服务器
            if (!co_await connect_to_server()) {
                g_stats.connections_failed++;;
                g_stats.active_clients--;
                co_return;
            }
            g_stats.connections_success++;
            
            // 2. 登录
            if (!co_await login()) {
                g_stats.logins_failed++;
                g_stats.active_clients--;
                co_return;
            }
            g_stats.logins_success++;
            
            // 3. 启动接收协程
            asio::co_spawn(socket_.get_executor(),
                [self]() -> asio::awaitable<void> { co_await self->receive_loop(); },
                asio::detached);
            
            // 4. 发送消息到世界频道
            co_await send_messages();
            
            // 5. 等待一段时间接收剩余消息
            co_await asio::steady_timer(socket_.get_executor(), 2s).async_wait(asio::use_awaitable);
            
        } catch (const std::exception& e) {
            // 静默处理异常
        }
        
        // 安全关闭 socket
        asio::error_code ec;
        socket_.close(ec);
        
        g_stats.active_clients--;
    }
    
private:
    /// 连接到服务器
    asio::awaitable<bool> connect_to_server() {
        try {
            asio::ip::tcp::resolver resolver(socket_.get_executor());
            auto endpoints = co_await resolver.async_resolve(
                SERVER_HOST, std::to_string(SERVER_PORT), asio::use_awaitable);
            
            co_await asio::async_connect(socket_, endpoints, asio::use_awaitable);
            co_return true;
        } catch (...) {
            co_return false;
        }
    }
    
    /// 登录
    asio::awaitable<bool> login() {
        try {
            auto login_msg = build_login_message(client_id_);
            co_await asio::async_write(socket_, 
                asio::buffer(login_msg), 
                asio::use_awaitable);
            
            // 读取响应
            asio::streambuf response_buf;
            co_await asio::async_read_until(socket_, response_buf, '\n', asio::use_awaitable);
            
            std::istream is(&response_buf);
            std::string response_line;
            std::getline(is, response_line);
            
            auto resp = LoginResponse::parse(response_line);
            
            if (resp.ok) {
                user_id_ = resp.user_id;
                world_conversation_id_ = resp.world_conversation_id;
                
                // 如果服务器没有返回 worldConversationId，使用默认值
                if (world_conversation_id_.empty()) {
                    world_conversation_id_ = "1";  // 默认世界频道 ID
                }

                // 如果存在群配置，按组映射会话
                if (group_cfg_ && !group_cfg_->conv_by_group.empty()) {
                    size_t group_idx = static_cast<size_t>(client_id_) / group_cfg_->group_size;
                    auto it = group_cfg_->conv_by_group.find(group_idx);
                    if (it != group_cfg_->conv_by_group.end()) {
                        world_conversation_id_ = it->second;
                    }
                }
                
                co_return true;
            } else {
                std::println("客户端 {} 登录失败: {}", client_id_, resp.error_msg);
                co_return false;
            }
        } catch (...) {
            co_return false;
        }
    }
    
    /// 消息接收循环
    asio::awaitable<void> receive_loop() {
        // 确保对象在接收循环期间存活
        auto self = shared_from_this();
        try {
            asio::streambuf buf;
            while (socket_.is_open()) {
                auto n = co_await asio::async_read_until(socket_, buf, '\n', asio::use_awaitable);
                
                std::istream is(&buf);
                std::string line;
                std::getline(is, line);
                
                if (is_send_ack(line)) {
                    // 解析 clientMsgId 用于延迟统计
                    std::string client_msg_id;
                    try {
                        auto payload = line.substr(std::string("SEND_ACK:").size());
                        auto j = nlohmann::json::parse(payload, nullptr, false);
                        if (!j.is_discarded() && j.contains("clientMsgId")) {
                            client_msg_id = j.at("clientMsgId").get<std::string>();
                        }
                    } catch (...) {
                        // ignore malformed ack
                    }

                    g_stats.messages_acked++;

                    if (!client_msg_id.empty()) {
                        std::lock_guard lock(pending_mutex_);
                        auto it = pending_sends_.find(client_msg_id);
                        if (it != pending_sends_.end()) {
                            auto latency = std::chrono::steady_clock::now() - it->second;
                            auto latency_us = std::chrono::duration_cast<std::chrono::microseconds>(latency).count();
                            g_stats.total_latency_us += latency_us;
                            g_stats.latency_samples++;
                            pending_sends_.erase(it);
                        }
                    }
                } else if (is_msg_push(line)) {
                    g_stats.messages_received++;
                }
            }
        } catch (...) {
            // 连接关闭
        }
    }
    
    /// 发送消息到世界频道 (全速无间隔)
    asio::awaitable<void> send_messages() {
        for (size_t i = 0; i < MESSAGES_PER_CLIENT; ++i) {
            try {
                auto send_time = std::chrono::steady_clock::now();
                auto client_msg_id = build_client_msg_id(user_id_, static_cast<int>(i));
                
                {
                    std::lock_guard lock(pending_mutex_);
                    pending_sends_[client_msg_id] = send_time;
                }

                auto msg = build_send_message(
                    world_conversation_id_, 
                    user_id_, 
                    static_cast<int>(i),
                    client_msg_id);
                co_await asio::async_write(socket_, 
                    asio::buffer(msg), 
                    asio::use_awaitable);
                
                g_stats.messages_sent++;
            } catch (...) {
                break;
            }
        }
    }
    
    asio::ip::tcp::socket socket_;
    int client_id_;
    std::string user_id_;
    std::string world_conversation_id_;

    // 追踪 clientMsgId -> 发送时间，用于 ACK 延迟统计
    std::mutex pending_mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> pending_sends_;

    const GroupConfig* group_cfg_{nullptr};
    std::mt19937 rng_{}; // no-op RNG, kept to minimize constructor changes
};

// ==================== 进度报告 ====================
std::atomic<bool> g_running{true};

void start_progress_timer(asio::any_io_executor exec) {
    auto timer = std::make_shared<asio::steady_timer>(exec);
    auto schedule_next = std::make_shared<std::function<void()>>();
    *schedule_next = [timer, schedule_next]() {
        if (!g_running.load()) return;
        
        timer->expires_after(1s);
        timer->async_wait([timer, schedule_next](const asio::error_code& ec) {
            if (!ec && g_running.load()) {
                g_stats.print_progress();
                (*schedule_next)();
            }
        });
    };
    (*schedule_next)();
}

// ==================== 主协程 ====================

asio::awaitable<void> run_world_test() {
    auto exec = co_await asio::this_coro::executor;
    auto group_cfg = load_group_config();
    
    std::println("===== 世界消息压测工具 (压力模式) =====");
    std::println("服务器: {}:{}", SERVER_HOST, SERVER_PORT);
    std::println("账号前缀: {}", ACCOUNT_PREFIX);
    std::println("客户端数: {} 个", NUM_CLIENTS);
    std::println("每客户端消息数: {} 条", MESSAGES_PER_CLIENT);
    std::println("总消息数: {} 条", NUM_CLIENTS * MESSAGES_PER_CLIENT);
    std::println("最大并发: {}", MESSAGE_MAX_CONCURRENT);
    std::println("消息间隔: 0 秒 (全速压测)");
    if (group_cfg && !group_cfg->conv_by_group.empty()) {
        std::println("群配置: 已加载 {} 个群，会话按分组发送", group_cfg->conv_by_group.size());
    } else {
        std::println("群配置: 未找到 benchmark_groups.json，默认使用世界频道");
    }
    std::println("=========================================\n");
    
    g_stats.start_time = std::chrono::steady_clock::now();
    g_running = true;
    
    // 启动进度报告
    start_progress_timer(exec);
    
    // 流水线模式：持续启动新连接，只限制最大并发数
    size_t next_id = 0;
    while (next_id < NUM_CLIENTS || g_stats.active_clients.load() > 0) {
        // 如果还有客户端要启动，且并发数未达上限，启动新连接
        while (next_id < NUM_CLIENTS && g_stats.active_clients.load() < MESSAGE_MAX_CONCURRENT) {
            const GroupConfig* cfg_ptr = group_cfg ? &(*group_cfg) : nullptr;
            auto client = std::make_shared<WorldTestClient>(exec, static_cast<int>(next_id), cfg_ptr);
            asio::co_spawn(exec, client->run(), asio::detached);
            next_id++;
        }
        
        // 短暂让出执行权
        co_await asio::steady_timer(exec, 1ms).async_wait(asio::use_awaitable);
    }
    
    // 停止进度报告
    g_running = false;
    
    // 打印最终统计
    g_stats.print_summary();
}

// ==================== 主函数 ====================

int main() {
    try {
        auto threads = std::max(4u, std::thread::hardware_concurrency());
        asio::thread_pool pool(threads);
        
        std::println("使用 {} 个工作线程\n", threads);
        
        asio::co_spawn(pool, run_world_test(), asio::detached);
        
        pool.join();
        
    } catch (const std::exception& e) {
        std::println(stderr, "错误: {}", e.what());
        return 1;
    }
    
    return 0;
}
