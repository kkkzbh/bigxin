#pragma once

#include <asio.hpp>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

#include <print>
#include <istream>
#include <memory>

#include <protocol.h>
#include <utility.h>
#include <database.h>

/// \brief 与聊天服务相关的网络组件。
/// \details 使用 asio::awaitable / use_awaitable 实现的协程式会话与服务器。
namespace server
{
    /// \brief 单个 TCP 连接会话，负责收发一条线路上的文本协议。
    struct Session
    {
        /// \brief 使用一个已建立连接的 socket 构造会话。
        /// \param socket 已经 accept 完成的 TCP socket。
        explicit Session(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

        /// \brief 以协程形式运行会话主循环。
        /// \details 使用 async_read_until / async_write 配合 use_awaitable。
        /// \return 协程完成时返回 void。
        auto run() -> asio::awaitable<void>
        {
            try {
                while(true) {
                    co_await asio::async_read_until(
                        socket_, buffer_, '\n', asio::use_awaitable
                    );

                    std::string line;
                    {
                        std::istream is{ &buffer_ };
                        std::getline(is, line);
                    }

                    if(line.empty()) {
                        continue;
                    }

                    auto frame = protocol::parse_line(line);
                    std::string response_payload;
                    std::string response_command;

                    if(frame.command == "PING") {
                        response_command = "PONG";
                        response_payload = "{}";
                    } else if(frame.command == "REGISTER") {
                        response_command = "REGISTER_RESP";
                        response_payload = handle_register(frame.payload);
                    } else if(frame.command == "LOGIN") {
                        response_command = "LOGIN_RESP";
                        response_payload = handle_login(frame.payload);
                    } else {
                        // 默认 echo，方便用 nc 观察未知命令。
                        response_command = "ECHO";
                        response_payload = "{\"command\":\"" + frame.command + "\"}";
                    }

                    auto msg = protocol::make_line(response_command, response_payload);
                    co_await asio::async_write(
                        socket_, asio::buffer(msg), asio::use_awaitable
                    );
                }
            } catch(std::exception const& ex) {
                std::println("session error: {}", ex.what());
            }
        }

    private:
        /// \brief 处理注册命令，返回 REGISTER_RESP 的 JSON 串。
        auto handle_register(std::string const& payload) -> std::string
        {
            using json = nlohmann::json;

            try {
                auto j = json::parse(payload);

                if(!j.contains("account") || !j.contains("password") || !j.contains("confirmPassword")) {
                    return make_error_payload("INVALID_PARAM", "缺少必要字段");
                }

                auto const account = j.at("account").get<std::string>();
                auto const password = j.at("password").get<std::string>();
                auto const confirm = j.at("confirmPassword").get<std::string>();

                if(password != confirm) {
                    return make_error_payload("PASSWORD_MISMATCH", "两次密码不一致");
                }

                auto const result = database::register_user(account, password);
                if(!result.ok) {
                    return make_error_payload(result.error_code, result.error_msg);
                }

                json resp;
                resp["ok"] = true;
                resp["userId"] = std::to_string(result.user.id);
                resp["displayName"] = result.user.display_name;
                return resp.dump();
            } catch(json::parse_error const&) {
                return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
            } catch(std::exception const& ex) {
                return make_error_payload("SERVER_ERROR", ex.what());
            }
        }

        /// \brief 处理登录命令，返回 LOGIN_RESP 的 JSON 串。
        auto handle_login(std::string const& payload) -> std::string
        {
            using json = nlohmann::json;

            try {
                auto j = json::parse(payload);

                if(!j.contains("account") || !j.contains("password")) {
                    return make_error_payload("INVALID_PARAM", "缺少必要字段");
                }

                auto const account = j.at("account").get<std::string>();
                auto const password = j.at("password").get<std::string>();

                auto const result = database::login_user(account, password);
                if(!result.ok) {
                    return make_error_payload(result.error_code, result.error_msg);
                }

                authenticated_ = true;
                user_id_ = result.user.id;
                account_ = result.user.account;
                display_name_ = result.user.display_name;

                json resp;
                resp["ok"] = true;
                resp["userId"] = std::to_string(user_id_);
                resp["displayName"] = display_name_;
                return resp.dump();
            } catch(json::parse_error const&) {
                return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
            } catch(std::exception const& ex) {
                return make_error_payload("SERVER_ERROR", ex.what());
            }
        }

        /// \brief 构造带错误码的通用错误响应 JSON 串。
        auto make_error_payload(std::string const& code, std::string const& msg) const -> std::string
        {
            nlohmann::json j;
            j["ok"] = false;
            j["errorCode"] = code;
            j["errorMsg"] = msg;
            return j.dump();
        }

        asio::ip::tcp::socket socket_;
        asio::streambuf buffer_;

        /// \brief 是否已通过 LOGIN 鉴权。
        bool authenticated_{ false };
        /// \brief 当前会话绑定的用户 ID。
        i64 user_id_{};
        /// \brief 当前会话绑定的账号。
        std::string account_{};
        /// \brief 当前会话绑定的昵称。
        std::string display_name_{};
    };

    /// \brief 简单 TCP 服务器：监听端口并为每个连接创建一个 Session。
    struct Server
    {
        /// \brief 使用给定执行器和端口构造服务器。
        /// \param exec 关联的 Asio 执行器。
        /// \param port 要监听的本地端口。
        Server(asio::any_io_executor exec, u16 port)
            : acceptor_(exec, asio::ip::tcp::endpoint{ asio::ip::tcp::v4(), port })
        {}

        /// \brief 启动异步 accept 协程。
        auto start_accept() -> void
        {
            asio::co_spawn(
                acceptor_.get_executor(),
                run(),
                asio::detached
            );
        }

    private:
        /// \brief 接收连接并为每个连接启动一个 Session 协程。
        /// \return 协程完成时返回 void（通常不会返回）。
        auto run() -> asio::awaitable<void>
        {
            try {
                while(true) {
                    auto socket = co_await acceptor_.async_accept(asio::use_awaitable);

                    auto const endpoint = socket.remote_endpoint();
                    auto const addr = endpoint.address().to_string();
                    auto const port = endpoint.port();
                    std::println("new connection from: {}:{}", addr, port);

                    asio::co_spawn(
                        acceptor_.get_executor(),
                        [socket = std::move(socket)]() mutable -> asio::awaitable<void> {
                            Session s{ std::move(socket) };
                            co_await s.run();
                        },
                        asio::detached
                    );
                }
            } catch(std::exception const& ex) {
                std::println("server accept loop error: {}", ex.what());
            }
        }

        asio::ip::tcp::acceptor acceptor_;
    };

} // namespace server

/// \brief 方便 main 调用的启动入口，内部维护单例 Server。
auto inline start_server(asio::any_io_executor exec, u16 port) -> void
{
    static std::unique_ptr<server::Server> server;
    if(!server) {
        server = std::make_unique<server::Server>(exec, port);
        server->start_accept();
    }
}
