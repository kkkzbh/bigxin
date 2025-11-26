/**
 * @file register_accounts.cpp
 * @brief 批量注册账号工具
 * 
 * 功能:
 * 1. 使用 REGISTER 协议批量注册测试账号
 * 2. 账号格式: {prefix}user_0, {prefix}user_1, ...
 * 3. 统计注册成功/失败数量
 * 
 * 使用方法:
 * 1. 修改 include/benchmark_config.h 中的 ACCOUNT_PREFIX 区分不同测试批次
 * 2. 先启动服务器
 * 3. 运行此工具注册账号
 * 4. 然后运行 world_message_test 进行消息测试
 */

#include "benchmark_config.h"
#include <boost/asio.hpp>
namespace asio = boost::asio;
#include <print>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;
using namespace benchmark;
using nlohmann::json;

// ==================== 统计数据 ====================
struct Statistics {
    std::atomic<size_t> success{0};
    std::atomic<size_t> failed{0};
    std::atomic<size_t> already_exists{0};         // 账号已存在（视为成功）
    std::atomic<size_t> in_progress{0};
    
    std::chrono::steady_clock::time_point start_time;
    
    void print_progress() const {
        auto total = success.load() + failed.load() + already_exists.load();
        std::println("[进度] 已处理: {}/{} | 成功: {} | 已存在: {} | 失败: {} | 并发: {}",
            total, NUM_ACCOUNTS, success.load(), already_exists.load(), 
            failed.load(), in_progress.load());
    }
    
    void print_summary() const {
        auto duration = std::chrono::steady_clock::now() - start_time;
        auto duration_sec = std::chrono::duration<double>(duration).count();
        
        std::println("\n========== 注册结果汇总 ==========");
        std::println("运行时间: {:.2f} 秒", duration_sec);
        std::println("注册成功: {} 个账号", success.load());
        std::println("已存在: {} 个账号", already_exists.load());
        std::println("注册失败: {} 个账号", failed.load());
        std::println("可用账号: {} 个 (成功 + 已存在)", success.load() + already_exists.load());
        std::println("速率: {:.1f} 个/秒", (success.load() + already_exists.load() + failed.load()) / duration_sec);
        std::println("===================================\n");
    }
};

// 全局统计对象
Statistics g_stats;
// 记录注册成功的 userId，索引与 user_id 一致
std::vector<std::string> g_user_ids(NUM_ACCOUNTS);
// 记录观测账号 userId
std::optional<std::string> g_observer_user_id;

// ==================== 协议工具函数 ====================

/// 构建账号名称
std::string build_account_name(int user_id) {
    return std::format("{}user_{}", ACCOUNT_PREFIX, user_id);
}

/// 构建注册消息
std::string build_register_message(int user_id) {
    auto account = build_account_name(user_id);
    return std::format(
        "REGISTER:{{\"account\":\"{}\",\"password\":\"{}\",\"confirmPassword\":\"{}\"}}\n",
        account, PASSWORD, PASSWORD);
}

/// 构建注册消息（指定账号名，用于观测账号）
std::string build_register_message_account(const std::string& account) {
    return std::format(
        "REGISTER:{{\"account\":\"{}\",\"password\":\"{}\",\"confirmPassword\":\"{}\"}}\n",
        account, PASSWORD, PASSWORD);
}

/// 解析注册响应
struct RegisterResponse {
    bool ok{false};
    bool already_exists{false};
    std::string user_id;
    std::string error_msg;
    
    static RegisterResponse parse(const std::string& line) {
        RegisterResponse resp;
        
        // 检查是否是 REGISTER_RESP
        if (line.find("REGISTER_RESP:") != 0) {
            resp.error_msg = "非 REGISTER_RESP 响应";
            return resp;
        }
        
        // 检查是否成功
        if (line.find("\"ok\":true") != std::string::npos) {
            resp.ok = true;
            auto pos = line.find("\"userId\":\"");
            if (pos != std::string::npos) {
                auto start = pos + std::string{"\"userId\":\""}.size();
                auto end = line.find("\"", start);
                if (end != std::string::npos) {
                    resp.user_id = line.substr(start, end - start);
                }
            }
            return resp;
        }
        
        // 检查是否账号已存在
        if (line.find("ACCOUNT_EXISTS") != std::string::npos) {
            resp.already_exists = true;
            return resp;
        }
        
        // 其他失败情况
        auto pos = line.find("\"errorMsg\":\"");
        if (pos != std::string::npos) {
            auto start = pos + 12;
            auto end = line.find("\"", start);
            if (end != std::string::npos) {
                resp.error_msg = line.substr(start, end - start);
            }
        }
        
        return resp;
    }
};

// ==================== 注册客户端 ====================

class RegisterClient : public std::enable_shared_from_this<RegisterClient> {
public:
    RegisterClient(asio::any_io_executor exec, int user_id)
        : socket_(exec)
        , user_id_(user_id)
    {}
    
    /// 执行注册流程
    asio::awaitable<void> run() {
        // 保持对象生命周期，防止协程尚未完成时对象被释放
        auto self = shared_from_this();
        
        try {
            // 1. 连接服务器
            asio::ip::tcp::resolver resolver(socket_.get_executor());
            auto endpoints = co_await resolver.async_resolve(
                SERVER_HOST, std::to_string(SERVER_PORT), asio::use_awaitable);
            
            co_await asio::async_connect(socket_, endpoints, asio::use_awaitable);
            g_stats.in_progress++;
            // 2. 发送注册请求
            auto register_msg = build_register_message(user_id_);
            co_await asio::async_write(socket_, 
                asio::buffer(register_msg), 
                asio::use_awaitable);
            
            // 3. 读取响应
            asio::streambuf response_buf;
            co_await asio::async_read_until(socket_, response_buf, '\n', asio::use_awaitable);
            
            std::istream is(&response_buf);
            std::string response_line;
            std::getline(is, response_line);
            
            // 4. 解析响应
            auto resp = RegisterResponse::parse(response_line);
            
            if (resp.ok) {
                g_stats.success++;
                if (user_id_ >=0 && static_cast<size_t>(user_id_) < g_user_ids.size()) {
                    g_user_ids[static_cast<size_t>(user_id_)] = resp.user_id;
                }
            } else if (resp.already_exists) {
                g_stats.already_exists++;
            } else {
                g_stats.failed++;
            }
            
        } catch (const std::exception& e) {
            g_stats.failed++;
            // 静默处理，避免大量输出
        }
        
        // 安全关闭 socket
        asio::error_code ec;
        socket_.close(ec);
        
        g_stats.in_progress--;
    }
    
private:
    asio::ip::tcp::socket socket_;
    int user_id_;
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

// ==================== 额外阶段：创建群并写出会话信息 ====================

/// 简单登录，返回 userId 和维持的已连接 socket（用于后续操作）
struct LoginResult {
    bool ok{false};
    std::string user_id;
    std::optional<asio::ip::tcp::socket> socket;
    std::string error;
};

asio::awaitable<LoginResult> login_once(const std::string& account) {
    auto exec = co_await asio::this_coro::executor;
    LoginResult result;
    result.socket.emplace(exec);
    try {
        asio::ip::tcp::resolver resolver(exec);
        auto endpoints = co_await resolver.async_resolve(
            SERVER_HOST, std::to_string(SERVER_PORT), asio::use_awaitable);
        co_await asio::async_connect(*result.socket, endpoints, asio::use_awaitable);

        auto login_msg = std::format(
            "LOGIN:{{\"account\":\"{}\",\"password\":\"{}\"}}\n", account, PASSWORD);
        co_await asio::async_write(*result.socket, asio::buffer(login_msg), asio::use_awaitable);

        asio::streambuf buf;
        co_await asio::async_read_until(*result.socket, buf, '\n', asio::use_awaitable);
        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        if (line.find("LOGIN_RESP:") != 0) {
            result.error = "not LOGIN_RESP";
            co_return result;
        }
        if (line.find("\"ok\":true") == std::string::npos) {
            result.error = "login failed";
            co_return result;
        }
        auto pos = line.find("\"userId\":\"");
        if (pos != std::string::npos) {
            auto start = pos + std::string{"\"userId\":\""}.size();
            auto end = line.find("\"", start);
            if (end != std::string::npos) {
                result.user_id = line.substr(start, end - start);
            }
        }
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    co_return result;
}

/// 创建群，返回 conversationId
asio::awaitable<std::optional<std::string>> create_group(
    asio::ip::tcp::socket& socket,
    std::vector<std::string> const& member_ids)
{
    try {
        json req;
        req["memberUserIds"] = member_ids;
        auto line = std::format("CREATE_GROUP_REQ:{}", req.dump());
        line.push_back('\n');
        co_await asio::async_write(socket, asio::buffer(line), asio::use_awaitable);

        asio::streambuf buf;
        // 服务器会先推送 CONV_LIST_RESP，再返回 CREATE_GROUP_RESP，容忍前面若干行
        for (int attempt = 0; attempt < 20; ++attempt) {
            co_await asio::async_read_until(socket, buf, '\n', asio::use_awaitable);
            std::istream is(&buf);
            std::string resp_line;
            std::getline(is, resp_line);
            if (resp_line.rfind("CREATE_GROUP_RESP:", 0) == 0) {
                if (resp_line.find("\"ok\":true") == std::string::npos) {
                    std::println("create_group resp not ok: {}", resp_line);
                    co_return std::nullopt;
                }
                auto pos = resp_line.find("\"conversationId\":\"");
                if (pos != std::string::npos) {
                    auto start = pos + std::string{"\"conversationId\":\""}.size();
                    auto end = resp_line.find("\"", start);
                    if (end != std::string::npos) {
                        co_return resp_line.substr(start, end - start);
                    }
                }
                std::println("create_group resp missing conversationId: {}", resp_line);
                co_return std::nullopt;
            } else {
                std::println("create_group ignore line: {}", resp_line);
            }
        }
        std::println("create_group timeout waiting resp");
    } catch (...) {
        co_return std::nullopt;
    }
    co_return std::nullopt;
}

/// 注册观测账号
asio::awaitable<void> register_observer() {
    auto exec = co_await asio::this_coro::executor;
    try {
        asio::ip::tcp::resolver resolver(exec);
        auto endpoints = co_await resolver.async_resolve(
            SERVER_HOST, std::to_string(SERVER_PORT), asio::use_awaitable);
        asio::ip::tcp::socket socket(exec);
        co_await asio::async_connect(socket, endpoints, asio::use_awaitable);
        auto msg = build_register_message_account(OBSERVER_ACCOUNT);
        co_await asio::async_write(socket, asio::buffer(msg), asio::use_awaitable);
        asio::streambuf buf;
        co_await asio::async_read_until(socket, buf, '\n', asio::use_awaitable);
        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        auto resp = RegisterResponse::parse(line);
        if (resp.ok || resp.already_exists) {
            g_observer_user_id = resp.user_id;
        }
        socket.close();
    } catch (...) {
        // ignore
    }

    // 如果因账号已存在但没拿到 userId，尝试登录获取
    if (!g_observer_user_id.has_value()) {
        auto login_res = co_await login_once(OBSERVER_ACCOUNT);
        if (login_res.ok) {
            g_observer_user_id = login_res.user_id;
            if (login_res.socket) {
                asio::error_code ec;
                login_res.socket->close(ec);
            }
        }
    }
}

/// 创建 10 个群并写入 json 文件
asio::awaitable<void> create_groups_file() {
    // 注册观测账号
    co_await register_observer();

    size_t group_count = NUM_ACCOUNTS / GROUP_SIZE;
    std::vector<std::string> conv_ids(group_count);
    auto exec = co_await asio::this_coro::executor;

    for (size_t g = 0; g < group_count; ++g) {
        size_t start = g * GROUP_SIZE;
        size_t end = std::min(start + GROUP_SIZE, NUM_ACCOUNTS);
        if (start >= end) break;

        // creator account
        auto creator_account = build_account_name(static_cast<int>(start));
        auto login_res = co_await login_once(creator_account);
        if (!login_res.ok) {
            std::println("group {} login failed: {}", g, login_res.error);
            continue;
        }

        // member list (string userIds)
        std::vector<std::string> members;
        members.reserve(end - start + 2);
        for (size_t i = start; i < end; ++i) {
            if (!g_user_ids[i].empty())
                members.push_back(g_user_ids[i]);
        }
        if (g_observer_user_id.has_value()) {
            members.push_back(*g_observer_user_id);
        }

        // 过滤空值，确保至少两人
        members.erase(std::remove_if(members.begin(), members.end(),
                                     [](auto const& s){ return s.empty(); }),
                      members.end());
        if (members.size() < 2) {
            std::println("group {} skip: members too few ({})", g, members.size());
            continue;
        }

        if (!login_res.socket) {
            std::println("group {} no socket", g);
            continue;
        }

        auto conv_id_opt = co_await create_group(*login_res.socket, members);
        if (conv_id_opt) {
            conv_ids[g] = *conv_id_opt;
            std::println("group {} created, convId={}", g, *conv_id_opt);
        } else {
            std::println("group {} create failed", g);
        }
        asio::error_code ec;
        if (login_res.socket) {
            login_res.socket->close(ec);
        }
    }

    // 写文件
    json out;
    out["groupSize"] = GROUP_SIZE;
    out["observerAccount"] = OBSERVER_ACCOUNT;
    out["groups"] = json::array();
    for (size_t g = 0; g < conv_ids.size(); ++g) {
        if (conv_ids[g].empty()) continue;
        json item;
        item["index"] = g;
        item["conversationId"] = conv_ids[g];
        out["groups"].push_back(std::move(item));
    }
    std::ofstream ofs("benchmark_groups.json");
    ofs << out.dump(2);
    std::println("写入群配置 benchmark_groups.json ，有效群数 {}", out["groups"].size());
}

// ==================== 主协程 ====================

asio::awaitable<void> register_all_accounts() {
    auto exec = co_await asio::this_coro::executor;
    
    std::println("===== 批量注册账号工具 (压力模式) =====");
    std::println("服务器: {}:{}", SERVER_HOST, SERVER_PORT);
    std::println("注册数量: {} 个账号", NUM_ACCOUNTS);
    std::println("账号前缀: {}", ACCOUNT_PREFIX);
    std::println("账号格式: {}user_0 ~ {}user_{}", ACCOUNT_PREFIX, ACCOUNT_PREFIX, NUM_ACCOUNTS - 1);
    std::println("统一密码: {}", PASSWORD);
    std::println("最大并发: {}", REGISTER_MAX_CONCURRENT);
    std::println("============================================\n");
    
    g_stats.start_time = std::chrono::steady_clock::now();
    g_running = true;
    
    // 启动进度报告
    start_progress_timer(exec);
    
    // 流水线模式：持续启动新连接，只限制最大并发数
    size_t next_id = 0;
    while (next_id < NUM_ACCOUNTS || g_stats.in_progress.load() > 0) {
        // 如果还有账号要注册，且并发数未达上限，启动新连接
        while (next_id < NUM_ACCOUNTS && g_stats.in_progress.load() < REGISTER_MAX_CONCURRENT) {
            auto client = std::make_shared<RegisterClient>(exec, static_cast<int>(next_id));
            asio::co_spawn(exec, client->run(), asio::detached);
            next_id++;
        }
        // 短暂让出执行权，让其他协程有机会运行
        co_await asio::steady_timer(exec, 1ms).async_wait(asio::use_awaitable);
    }
    
    // 停止进度报告
    g_running = false;
    
    // 打印最终统计
    g_stats.print_summary();

    // 创建群并写出配置
    co_await create_groups_file();
}

// ==================== 主函数 ====================

#include <thread>

int main() {
    try {
        auto NUM_THREADS = std::thread::hardware_concurrency();
        asio::thread_pool pool(NUM_THREADS);
        
        asio::co_spawn(pool, register_all_accounts(), asio::detached);
        
        pool.join();
        
    } catch (const std::exception& e) {
        std::println(stderr, "错误: {}", e.what());
        return 1;
    }
    
    return 0;
}
