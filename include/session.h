#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
namespace asio = boost::asio;

#include <nlohmann/json.hpp>

#include <print>
#include <deque>
#include <istream>
#include <memory>
#include <string>
#include <cctype>

#include <protocol.h>
#include <utility.h>

/// \brief 与聊天会话相关的网络组件。
/// \details 使用 asio::awaitable / use_awaitable 实现的协程式会话。
struct Server;

/// \brief 单个 TCP 连接会话，负责收发一条线路上的文本协议。
struct Session : std::enable_shared_from_this<Session>
{
    friend struct Server;

    /// \brief 使用一个已建立连接的 socket 构造会话。
    /// \param socket 已经 accept 完成的 TCP socket。
    /// \param server 当前所属的 Server，用于后续广播。
    explicit Session(asio::ip::tcp::socket socket, Server& server)
        : socket_(std::move(socket))
        , server_(&server)
    {}

    /// \brief 压缩连续空白并去掉首尾空格，避免昵称中有填充空格。
    static auto normalize_whitespace(std::string s) -> std::string
    {
        std::string out;
        out.reserve(s.size());
        bool pending_space = false;
        for(char ch : s) {
            if(std::isspace(static_cast<unsigned char>(ch))) {
                pending_space = !out.empty(); // 仅在已有内容后记录空格
                continue;
            }
            if(pending_space) {
                out.push_back(' ');
                pending_space = false;
            }
            out.push_back(ch);
        }
        while(!out.empty() && out.back() == ' ') {
            out.pop_back();
        }
        return out;
    }

    /// \brief 以协程形式运行会话主循环。
    /// \details 使用 async_read_until / async_write 配合 use_awaitable。
    /// \return 协程完成时返回 void。
    auto run() -> asio::awaitable<void>
    {
        try {
            while(true) {
                co_await asio::async_read_until (
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
                if(frame.command == "PING") {
                    auto msg = protocol::make_line("PONG", "{}");
                    send_text(std::move(msg));
                } else if(frame.command == "REGISTER") {
                    auto payload = co_await handle_register(frame.payload);
                    auto msg = protocol::make_line("REGISTER_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "LOGIN") {
                    auto payload = co_await handle_login(frame.payload);
                    auto msg = protocol::make_line("LOGIN_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "SEND_MSG") {
                    auto self = shared_from_this();
                    asio::co_spawn(
                        socket_.get_executor(),
                        [self, payload = frame.payload]() mutable -> asio::awaitable<void> {
                            co_await self->handle_send_msg(std::move(payload));
                        },
                        asio::detached
                    );
                } else if(frame.command == "HISTORY_REQ") {
                    auto payload = co_await handle_history_req(frame.payload);
                    auto msg = protocol::make_line("HISTORY_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "CONV_LIST_REQ") {
                    auto payload = co_await handle_conv_list_req(frame.payload);
                    auto msg = protocol::make_line("CONV_LIST_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "PROFILE_UPDATE") {
                    auto payload = co_await handle_profile_update(frame.payload);
                    auto msg = protocol::make_line("PROFILE_UPDATE_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "FRIEND_LIST_REQ") {
                    auto payload = co_await handle_friend_list_req(frame.payload);
                    auto msg = protocol::make_line("FRIEND_LIST_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "FRIEND_SEARCH_REQ") {
                    auto payload = co_await handle_friend_search_req(frame.payload);
                    auto msg = protocol::make_line("FRIEND_SEARCH_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "FRIEND_ADD_REQ") {
                    auto payload = co_await handle_friend_add_req(frame.payload);
                    auto msg = protocol::make_line("FRIEND_ADD_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "FRIEND_REQ_LIST_REQ") {
                    auto payload = co_await handle_friend_req_list_req(frame.payload);
                    auto msg = protocol::make_line("FRIEND_REQ_LIST_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "FRIEND_ACCEPT_REQ") {
                    auto payload = co_await handle_friend_accept_req(frame.payload);
                    auto msg = protocol::make_line("FRIEND_ACCEPT_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "CREATE_GROUP_REQ") {
                    auto payload = co_await handle_create_group_req(frame.payload);
                    auto msg = protocol::make_line("CREATE_GROUP_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "OPEN_SINGLE_CONV_REQ") {
                    auto payload = co_await handle_open_single_conv_req(frame.payload);
                    auto msg = protocol::make_line("OPEN_SINGLE_CONV_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "MUTE_MEMBER_REQ") {
                    auto payload = co_await handle_mute_member_req(frame.payload);
                    auto msg = protocol::make_line("MUTE_MEMBER_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "UNMUTE_MEMBER_REQ") {
                    auto payload = co_await handle_unmute_member_req(frame.payload);
                    auto msg = protocol::make_line("UNMUTE_MEMBER_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "CONV_MEMBERS_REQ") {
                    auto payload = co_await handle_conv_members_req(frame.payload);
                    auto msg = protocol::make_line("CONV_MEMBERS_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "LEAVE_CONV_REQ") {
                    auto payload = co_await handle_leave_conv_req(frame.payload);
                    auto msg = protocol::make_line("LEAVE_CONV_RESP", payload);
                    send_text(std::move(msg));
                } else {
                    // 默认 echo，方便用 nc 观察未知命令。
                    auto payload = std::string{ "{\"command\":\"" + frame.command + "\"}" };
                    auto msg = protocol::make_line("ECHO", payload);
                    send_text(std::move(msg));
                }
            }
        } catch(boost::system::system_error const& ex) {
            if(ex.code() == asio::error::eof) {
                std::println("session closed by peer");
            } else {
                std::println("session error: {}", ex.what());
            }
        } catch(std::exception const& ex) {
            std::println("session error: {}", ex.what());
        }
    }

private:
    /// \brief 处理注册命令，返回 REGISTER_RESP 的 JSON 串。
    auto handle_register(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理登录命令，返回 LOGIN_RESP 的 JSON 串。
    auto handle_login(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理发送消息命令，在“世界”会话中写入并广播。
    /// \param payload SEND_MSG 的 JSON 文本。
    /// \note 异步协程，在数据库线程池上执行阻塞操作。
    auto handle_send_msg(std::string payload) -> asio::awaitable<void>;

    /// \brief 处理历史消息请求，返回 HISTORY_RESP 的 JSON 串。
    /// \param payload HISTORY_REQ 的 JSON 文本。
    auto handle_history_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理会话列表请求，返回 CONV_LIST_RESP 的 JSON 串。
    /// \param payload CONV_LIST_REQ 的 JSON 文本。
    auto handle_conv_list_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理资料更新请求，返回 PROFILE_UPDATE_RESP 的 JSON 串。
    /// \param payload PROFILE_UPDATE 的 JSON 文本。
    auto handle_profile_update(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理好友列表请求，返回 FRIEND_LIST_RESP 的 JSON 串。
    /// \param payload FRIEND_LIST_REQ 的 JSON 文本。
    auto handle_friend_list_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理按账号搜索好友的请求，返回 FRIEND_SEARCH_RESP 的 JSON 串。
    /// \param payload FRIEND_SEARCH_REQ 的 JSON 文本。
    auto handle_friend_search_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理创建好友申请的请求，返回 FRIEND_ADD_RESP 的 JSON 串。
    /// \param payload FRIEND_ADD_REQ 的 JSON 文本。
    auto handle_friend_add_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理“新的朋友”列表请求，返回 FRIEND_REQ_LIST_RESP 的 JSON 串。
    /// \param payload FRIEND_REQ_LIST_REQ 的 JSON 文本。
    auto handle_friend_req_list_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理同意好友申请的请求，返回 FRIEND_ACCEPT_RESP 的 JSON 串。
    /// \param payload FRIEND_ACCEPT_REQ 的 JSON 文本。
    auto handle_friend_accept_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理创建群聊的请求，返回 CREATE_GROUP_RESP 的 JSON 串。
    /// \param payload CREATE_GROUP_REQ 的 JSON 文本。
    auto handle_create_group_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理打开单聊会话的请求，返回 OPEN_SINGLE_CONV_RESP 的 JSON 串。
    /// \param payload OPEN_SINGLE_CONV_REQ 的 JSON 文本。
    auto handle_open_single_conv_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理群成员禁言请求。
    auto handle_mute_member_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理群成员解禁请求。
    auto handle_unmute_member_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理会话成员列表请求。
    auto handle_conv_members_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理退群 / 解散群聊请求。
    /// \param payload LEAVE_CONV_REQ 的 JSON 文本。
    auto handle_leave_conv_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 构造带错误码的通用错误响应 JSON 串。
    auto make_error_payload(std::string const& code, std::string const& msg) const -> std::string
    {
        nlohmann::json j;
        j["ok"] = false;
        j["errorCode"] = code;
        j["errorMsg"] = msg;
        return j.dump();
    }

    /// \brief 向当前会话异步发送一行文本。
    /// \param line 已经包含换行符的完整协议行。
    auto send_text(std::string line) -> void
    {
        // 检查缓冲区是否超限,防止慢客户端导致内存无限增长
        if(outgoing_bytes_ + line.size() > MAX_OUTGOING_BYTES) {
            std::println("session write buffer overflow ({}MB), closing connection",
                        (outgoing_bytes_ + line.size()) / (1024 * 1024));
            socket_.close();
            return;
        }
        
        outgoing_bytes_ += line.size();
        outgoing_.push_back(std::move(line));
        if(writing_) {
            return;
        }

        writing_ = true;
        auto self = shared_from_this();

        asio::co_spawn(
            socket_.get_executor(),
            [self]() -> asio::awaitable<void> {
                try {
                    while(!self->outgoing_.empty()) {
                        auto current = std::move(self->outgoing_.front());
                        self->outgoing_.pop_front();
                        self->outgoing_bytes_ -= current.size();
                        co_await asio::async_write (
                            self->socket_, asio::buffer(current), asio::use_awaitable
                        );
                    }
                } catch(std::exception const& ex) {
                    std::println("session write error: {}", ex.what());
                }
                self->writing_ = false;
            },
            asio::detached
        );
    }

    /// \brief 是否已通过 LOGIN 鉴权。
    auto is_authenticated() const noexcept -> bool
    {
        return authenticated_;
    }

    /// \brief 当前用户 ID（未登录时为 0）。
    auto user_id() const noexcept -> i64
    {
        return user_id_;
    }

    asio::ip::tcp::socket socket_;
    asio::streambuf buffer_;
    Server* server_{ nullptr };
    std::deque<std::string> outgoing_{};
    size_t outgoing_bytes_{ 0 }; ///< 当前缓冲区总字节数
    static constexpr size_t MAX_OUTGOING_BYTES = 10 * 1024 * 1024; ///< 最大缓冲区 10MB
    bool writing_{ false };

    /// \brief 是否已通过 LOGIN 鉴权。
    bool authenticated_{ false };
    /// \brief 当前会话绑定的用户 ID。
    i64 user_id_{};
    /// \brief 当前会话绑定的账号。
    std::string account_{};
    /// \brief 当前会话绑定的昵称。
    std::string display_name_{};
};
