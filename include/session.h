#pragma once

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
namespace asio = boost::asio;

#include <nlohmann/json.hpp>

#include <print>
#include <deque>
#include <istream>
#include <memory>
#include <string>
#include <cctype>
#include <atomic>

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
    explicit Session(asio::ip::tcp::socket socket, std::shared_ptr<Server> server)
        : socket_(std::move(socket))
        , strand_(socket_.get_executor())
        , server_(std::move(server))
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
                    ++self->pending_ops_;  // 增加未完成操作计数
                    asio::co_spawn(
                        strand_,  // 使用 strand_ 而非 socket_.get_executor()，避免 socket 关闭后 executor 失效
                        [self, payload = frame.payload]() mutable -> asio::awaitable<void> {
                            // RAII 守卫：确保无论如何都会减少计数
                            struct PendingGuard {
                                std::shared_ptr<Session> s;
                                ~PendingGuard() { --s->pending_ops_; }
                            } guard{ self };
                            
                            try {
                                // 检查 session 是否正在关闭
                                if(self->closing_.load() || !self->socket_.is_open()) {
                                    co_return;
                                }
                                co_await self->handle_send_msg(std::move(payload));
                            } catch (std::exception const& e) {
                                std::println("SEND_MSG unhandled exception: {}", e.what());
                            }
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
                } else if(frame.command == "AVATAR_UPDATE") {
                    auto payload = co_await handle_avatar_update(frame.payload);
                    auto msg = protocol::make_line("AVATAR_UPDATE_RESP", payload);
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
                } else if(frame.command == "FRIEND_REJECT_REQ") {
                    auto payload = co_await handle_friend_reject_req(frame.payload);
                    auto msg = protocol::make_line("FRIEND_REJECT_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "FRIEND_DELETE_REQ") {
                    auto payload = co_await handle_friend_delete_req(frame.payload);
                    auto msg = protocol::make_line("FRIEND_DELETE_RESP", payload);
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
                } else if(frame.command == "SET_ADMIN_REQ") {
                    auto payload = co_await handle_set_admin_req(frame.payload);
                    auto msg = protocol::make_line("SET_ADMIN_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "CONV_MEMBERS_REQ") {
                    auto payload = co_await handle_conv_members_req(frame.payload);
                    auto msg = protocol::make_line("CONV_MEMBERS_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "LEAVE_CONV_REQ") {
                    auto payload = co_await handle_leave_conv_req(frame.payload);
                    auto msg = protocol::make_line("LEAVE_CONV_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "GROUP_SEARCH_REQ") {
                    auto payload = co_await handle_group_search_req(frame.payload);
                    auto msg = protocol::make_line("GROUP_SEARCH_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "GROUP_JOIN_REQ") {
                    auto payload = co_await handle_group_join_req(frame.payload);
                    auto msg = protocol::make_line("GROUP_JOIN_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "GROUP_JOIN_REQ_LIST_REQ") {
                    auto payload = co_await handle_group_join_req_list_req(frame.payload);
                    auto msg = protocol::make_line("GROUP_JOIN_REQ_LIST_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "GROUP_JOIN_ACCEPT_REQ") {
                    auto payload = co_await handle_group_join_accept_req(frame.payload);
                    auto msg = protocol::make_line("GROUP_JOIN_ACCEPT_RESP", payload);
                    send_text(std::move(msg));
                } else if(frame.command == "RENAME_GROUP_REQ") {
                    auto payload = co_await handle_rename_group_req(frame.payload);
                    auto msg = protocol::make_line("RENAME_GROUP_RESP", payload);
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
            } else if(ex.code() == asio::error::connection_reset) {
                std::println("session connection reset by peer");
            } else if(ex.code() == asio::error::operation_aborted) {
                std::println("session operation aborted");
            } else {
                std::println("session error: {} ({})", ex.what(), ex.code().value());
            }
        } catch(std::exception const& ex) {
            std::println("session error: {}", ex.what());
        }
        
        // 标记会话正在关闭，阻止新的异步操作启动
        closing_.store(true);
        
        // 等待所有未完成的异步操作（如 handle_send_msg）完成
        // 使用 strand_ 而不是 socket_.get_executor()，因为 socket 可能已关闭/失效
        // strand_ 是从 io_context 派生的，即使 socket 关闭也仍然有效
        for(int i = 0; i < 200 && pending_ops_.load() > 0; ++i) {
            asio::steady_timer wait_timer{ strand_ };
            wait_timer.expires_after(std::chrono::milliseconds{ 10 });
            boost::system::error_code ec;
            co_await wait_timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if(ec == asio::error::operation_aborted) {
                // io_context 正在关闭，不再等待
                break;
            }
        }
        
        if(pending_ops_.load() > 0) {
            std::println("session closing with {} pending ops (timeout)", pending_ops_.load());
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

    /// \brief 处理头像更新请求，返回 AVATAR_UPDATE_RESP 的 JSON 串。
    /// \param payload AVATAR_UPDATE 的 JSON 文本。
    auto handle_avatar_update(std::string const& payload) -> asio::awaitable<std::string>;

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

    /// \brief 处理拒绝好友申请的请求，返回 FRIEND_REJECT_RESP 的 JSON 串。
    /// \param payload FRIEND_REJECT_REQ 的 JSON 文本。
    auto handle_friend_reject_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理删除好友的请求，返回 FRIEND_DELETE_RESP 的 JSON 串。
    /// \param payload FRIEND_DELETE_REQ 的 JSON 文本。
    auto handle_friend_delete_req(std::string const& payload) -> asio::awaitable<std::string>;

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

    /// \brief 处理设置/取消管理员请求。
    auto handle_set_admin_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理会话成员列表请求。
    auto handle_conv_members_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理退群 / 解散群聊请求。
    /// \param payload LEAVE_CONV_REQ 的 JSON 文本。
    auto handle_leave_conv_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理群聊搜索请求，返回 GROUP_SEARCH_RESP 的 JSON 串。
    /// \param payload GROUP_SEARCH_REQ 的 JSON 文本。
    auto handle_group_search_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理入群申请请求，返回 GROUP_JOIN_RESP 的 JSON 串。
    /// \param payload GROUP_JOIN_REQ 的 JSON 文本。
    auto handle_group_join_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理入群申请列表请求，返回 GROUP_JOIN_REQ_LIST_RESP 的 JSON 串。
    /// \param payload GROUP_JOIN_REQ_LIST_REQ 的 JSON 文本。
    auto handle_group_join_req_list_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理入群申请（同意/拒绝），返回 GROUP_JOIN_ACCEPT_RESP 的 JSON 串。
    /// \param payload GROUP_JOIN_ACCEPT_REQ 的 JSON 文本。
    auto handle_group_join_accept_req(std::string const& payload) -> asio::awaitable<std::string>;

    /// \brief 处理群组重命名请求，返回 RENAME_GROUP_RESP 的 JSON 串。
    /// \param payload RENAME_GROUP_REQ 的 JSON 文本。
    auto handle_rename_group_req(std::string const& payload) -> asio::awaitable<std::string>;

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
        // 将所有对 outgoing_ 的访问都放在 strand 上执行，保证线程安全
        asio::dispatch(strand_, [this, self = shared_from_this(), line = std::move(line)]() mutable {
            send_text_impl(std::move(line));
        });
    }

private:
    /// \brief send_text 的实际实现，必须在 strand_ 上调用。
    auto send_text_impl(std::string line) -> void
    {
        // 如果 socket 已关闭，直接返回
        if(!socket_.is_open()) {
            return;
        }
        
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
            strand_,
            [self]() -> asio::awaitable<void> {
                try {
                    while(!self->outgoing_.empty() && self->socket_.is_open()) {
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

public:

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
    /// \brief strand 保证 outgoing_ 队列的线程安全访问。
    asio::strand<asio::any_io_executor> strand_;
    asio::streambuf buffer_;
    std::weak_ptr<Server> server_; ///< 所属服务器的弱引用，避免服务器销毁后悬垂指针。
    std::deque<std::string> outgoing_{};
    size_t outgoing_bytes_{ 0 }; ///< 当前缓冲区总字节数
    static constexpr size_t MAX_OUTGOING_BYTES = 10 * 1024 * 1024; ///< 最大缓冲区 10MB
    bool writing_{ false };
    
    /// \brief 追踪未完成的异步操作数量（如 handle_send_msg）。
    std::atomic<int> pending_ops_{ 0 };
    /// \brief 会话是否正在关闭中。
    std::atomic<bool> closing_{ false };

    /// \brief 是否已通过 LOGIN 鉴权。
    bool authenticated_{ false };
    /// \brief 当前会话绑定的用户 ID。
    i64 user_id_{};
    /// \brief 当前会话绑定的账号。
    std::string account_{};
    /// \brief 当前会话绑定的昵称。
    std::string display_name_{};
    /// \brief 当前会话绑定的头像路径。
    std::string avatar_path_{};
};
