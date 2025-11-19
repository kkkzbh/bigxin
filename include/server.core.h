#pragma once

#include <asio.hpp>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

#include <print>
#include <deque>
#include <istream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

#include <protocol.h>
#include <utility.h>
#include <database.h>

/// \brief 与聊天服务相关的网络组件。
/// \details 使用 asio::awaitable / use_awaitable 实现的协程式会话与服务器。
namespace server
{
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
                    if(frame.command == "PING") {
                        auto msg = protocol::make_line("PONG", "{}");
                        send_text(std::move(msg));
                    } else if(frame.command == "REGISTER") {
                        auto payload = handle_register(frame.payload);
                        auto msg = protocol::make_line("REGISTER_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "LOGIN") {
                        auto payload = handle_login(frame.payload);
                        auto msg = protocol::make_line("LOGIN_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "SEND_MSG") {
                        handle_send_msg(frame.payload);
                    } else if(frame.command == "HISTORY_REQ") {
                        auto payload = handle_history_req(frame.payload);
                        auto msg = protocol::make_line("HISTORY_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "CONV_LIST_REQ") {
                        auto payload = handle_conv_list_req(frame.payload);
                        auto msg = protocol::make_line("CONV_LIST_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "PROFILE_UPDATE") {
                        auto payload = handle_profile_update(frame.payload);
                        auto msg = protocol::make_line("PROFILE_UPDATE_RESP", payload);
                        send_text(std::move(msg));
                    } else {
                        // 默认 echo，方便用 nc 观察未知命令。
                        auto payload = std::string{ "{\"command\":\"" + frame.command + "\"}" };
                        auto msg = protocol::make_line("ECHO", payload);
                        send_text(std::move(msg));
                    }
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

                auto const world_id = database::get_world_conversation_id();

                json resp;
                resp["ok"] = true;
                resp["userId"] = std::to_string(user_id_);
                resp["displayName"] = display_name_;
                resp["worldConversationId"] = std::to_string(world_id);
                return resp.dump();
            } catch(json::parse_error const&) {
                return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
            } catch(std::exception const& ex) {
                return make_error_payload("SERVER_ERROR", ex.what());
            }
        }

        /// \brief 处理发送消息命令，在“世界”会话中写入并广播。
        /// \param payload SEND_MSG 的 JSON 文本。
        auto handle_send_msg(std::string const& payload) -> void;

        /// \brief 处理历史消息请求，返回 HISTORY_RESP 的 JSON 串。
        /// \param payload HISTORY_REQ 的 JSON 文本。
        auto handle_history_req(std::string const& payload) -> std::string;

        /// \brief 处理会话列表请求，返回 CONV_LIST_RESP 的 JSON 串。
        /// \param payload CONV_LIST_REQ 的 JSON 文本。
        auto handle_conv_list_req(std::string const& payload) -> std::string;

        /// \brief 处理资料更新请求，返回 PROFILE_UPDATE_RESP 的 JSON 串。
        /// \param payload PROFILE_UPDATE 的 JSON 文本。
        auto handle_profile_update(std::string const& payload) -> std::string;

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
                            co_await asio::async_write(
                                self->socket_, asio::buffer(current), asio::use_awaitable
                            );
                        }
                    } catch(std::exception const& ex) {
                        std::println("session write error: {}", ex.what());
                    }
                    self->writing_ = false;
                    co_return;
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
        friend struct Session;

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

                    auto session = std::make_shared<Session>(std::move(socket), *this);
                    sessions_.push_back(session);

                    asio::co_spawn(
                        acceptor_.get_executor(),
                        [this, session]() -> asio::awaitable<void> {
                            co_await session->run();
                            remove_session(session.get());
                            co_return;
                        },
                        asio::detached
                    );
                }
            } catch(std::exception const& ex) {
                std::println("server accept loop error: {}", ex.what());
            }
        }

        asio::ip::tcp::acceptor acceptor_;
        std::vector<std::shared_ptr<Session>> sessions_{};

        /// \brief 从会话列表中移除一个已经结束的 Session。
        auto remove_session(Session* ptr) -> void
        {
            auto it = std::remove_if(
                sessions_.begin(),
                sessions_.end(),
                [ptr](std::shared_ptr<Session> const& s) { return s.get() == ptr; }
            );
            sessions_.erase(it, sessions_.end());
        }

        /// \brief 在指定会话中广播一条消息（群聊 / 单聊通用）。
        /// \param stored 已持久化的消息信息。
        /// \param sender_id 发送者用户 ID。
        /// \param content 消息文本内容。
        auto broadcast_world_message(
            database::StoredMessage const& stored,
            i64 sender_id,
            std::string const& content
        ) -> void
        {
            using json = nlohmann::json;

            json push;
            push["conversationId"] = std::to_string(stored.conversation_id);
            // 会话类型用于前端展示，不影响路由逻辑。
            std::string conv_type{ "GROUP" };
            try {
                auto conn = database::make_connection();
                pqxx::work tx{ conn };
                auto const query =
                    "SELECT type FROM conversations WHERE id = "
                    + tx.quote(stored.conversation_id) + " LIMIT 1";
                auto rows = tx.exec(query);
                if(!rows.empty()) {
                    conv_type = rows[0][0].as<std::string>();
                }
                tx.commit();
            } catch(std::exception const&) {
                // 保底使用 GROUP。
            }
            push["conversationType"] = conv_type;
            push["serverMsgId"] = std::to_string(stored.id);
            push["senderId"] = std::to_string(sender_id);
            push["msgType"] = "TEXT";
            push["serverTimeMs"] = stored.server_time_ms;
            push["seq"] = stored.seq;
            push["content"] = content;

            auto const line = protocol::make_line("MSG_PUSH", push.dump());

            // 仅向会话成员中在线的用户广播。
            std::vector<i64> member_ids;
            try {
                auto conn = database::make_connection();
                pqxx::work tx{ conn };
                auto const query =
                    "SELECT user_id FROM conversation_members WHERE conversation_id = "
                    + tx.quote(stored.conversation_id);
                auto rows = tx.exec(query);
                member_ids.reserve(rows.size());
                for(auto const& row : rows) {
                    member_ids.push_back(row[0].as<i64>());
                }
                tx.commit();
            } catch(std::exception const&) {
                // 如果查询失败，退化为广播给所有在线用户。
                member_ids.clear();
            }

            auto const is_target = [&member_ids](i64 uid) -> bool {
                if(member_ids.empty()) {
                    return true;
                }
                return std::find(member_ids.begin(), member_ids.end(), uid)
                       != member_ids.end();
            };

            for(auto const& session : sessions_) {
                if(session && session->is_authenticated() && is_target(session->user_id())) {
                    session->send_text(line);
                }
            }
        }
    };

} // namespace server

inline auto server::Session::handle_send_msg(std::string const& payload) -> void
{
    using json = nlohmann::json;

    if(!authenticated_) {
        std::println("SEND_MSG from unauthenticated session ignored");
        return;
    }

    try {
        auto j = json::parse(payload);

        if(!j.contains("content")) {
            auto const err = make_error_payload("INVALID_PARAM", "缺少 content 字段");
            auto msg = protocol::make_line("ERROR", err);
            send_text(std::move(msg));
            return;
        }

        auto conversation_id = i64{};
        if(j.contains("conversationId")) {
            auto const conv_str = j.at("conversationId").get<std::string>();
            if(!conv_str.empty()) {
                conversation_id = std::stoll(conv_str);
            }
        }
        if(conversation_id <= 0) {
            conversation_id = database::get_world_conversation_id();
        }

        auto const content = j.at("content").get<std::string>();
        auto const client_msg_id =
            j.contains("clientMsgId") ? j.at("clientMsgId").get<std::string>() : "";

        auto const stored = database::append_text_message(conversation_id, user_id_, content);

        json ack;
        ack["clientMsgId"] = client_msg_id;
        ack["serverMsgId"] = std::to_string(stored.id);
        ack["serverTimeMs"] = stored.server_time_ms;
        ack["seq"] = stored.seq;

        auto ack_msg = protocol::make_line("SEND_ACK", ack.dump());
        send_text(std::move(ack_msg));

        if(server_ != nullptr) {
            server_->broadcast_world_message(stored, user_id_, content);
        }
    } catch(json::parse_error const&) {
        auto const err = make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
        auto msg = protocol::make_line("ERROR", err);
        send_text(std::move(msg));
    } catch(std::exception const& ex) {
        auto const err = make_error_payload("SERVER_ERROR", ex.what());
        auto msg = protocol::make_line("ERROR", err);
        send_text(std::move(msg));
    }
}

inline auto server::Session::handle_history_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = json::parse(payload);

        auto before_seq = i64{};
        auto after_seq = i64{};
        auto limit = i64{ 50 };

        if(j.contains("beforeSeq")) {
            before_seq = j.at("beforeSeq").get<i64>();
        }
        if(j.contains("afterSeq")) {
            after_seq = j.at("afterSeq").get<i64>();
        }
        if(j.contains("limit")) {
            limit = j.at("limit").get<i64>();
        }

        auto conversation_id = i64{};
        if(j.contains("conversationId")) {
            auto const conv_str = j.at("conversationId").get<std::string>();
            if(!conv_str.empty()) {
                conversation_id = std::stoll(conv_str);
            }
        }
        if(conversation_id <= 0) {
            conversation_id = database::get_world_conversation_id();
        }

        std::vector<database::LoadedMessage> messages;
        if(after_seq > 0) {
            messages = database::load_user_conversation_since(conversation_id, after_seq, limit);
        } else {
            messages =
                database::load_user_conversation_history(conversation_id, before_seq, limit);
        }

        json resp;
        resp["conversationId"] = std::to_string(conversation_id);

        json items = json::array();
        for(auto const& msg : messages) {
            json m;
            m["serverMsgId"] = std::to_string(msg.id);
            m["senderId"] = std::to_string(msg.sender_id);
            m["msgType"] = msg.msg_type;
            m["serverTimeMs"] = msg.server_time_ms;
            m["seq"] = msg.seq;
            m["content"] = msg.content;
            items.push_back(std::move(m));
        }

        resp["messages"] = std::move(items);
        resp["hasMore"] = static_cast<i64>(messages.size()) >= limit;
        resp["nextBeforeSeq"] = messages.empty() ? 0 : messages.front().seq;

        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_conv_list_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            // 预留将来扩展过滤参数，目前仅校验 JSON 格式。
            (void)json::parse(payload);
        }

        auto const conversations = database::load_user_conversations(user_id_);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& conv : conversations) {
            json c;
            c["conversationId"] = std::to_string(conv.id);
            c["conversationType"] = conv.type;
            c["title"] = conv.title;
            c["lastSeq"] = conv.last_seq;
            c["lastServerTimeMs"] = conv.last_server_time_ms;
            items.push_back(std::move(c));
        }

        resp["conversations"] = std::move(items);
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_profile_update(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = json::parse(payload);

        if(!j.contains("displayName")) {
            return make_error_payload("INVALID_PARAM", "缺少 displayName 字段");
        }

        auto new_name = j.at("displayName").get<std::string>();

        // 去掉首尾空白并做简单长度校验。
        auto const trim = [](std::string& s) {
            auto const not_space = [](unsigned char ch) { return !std::isspace(ch); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
            s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        };
        trim(new_name);

        if(new_name.empty()) {
            return make_error_payload("INVALID_PARAM", "昵称不能为空");
        }
        if(new_name.size() > 64) {
            return make_error_payload("INVALID_PARAM", "昵称长度过长");
        }

        auto const result = database::update_display_name(user_id_, new_name);
        if(!result.ok) {
            return make_error_payload(result.error_code, result.error_msg);
        }

        // 更新当前会话缓存的昵称。
        display_name_ = result.user.display_name;

        json resp;
        resp["ok"] = true;
        resp["displayName"] = display_name_;
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

/// \brief 方便 main 调用的启动入口，内部维护单例 Server。
auto inline start_server(asio::any_io_executor exec, u16 port) -> void
{
    static std::unique_ptr<server::Server> server;
    if(!server) {
        server = std::make_unique<server::Server>(exec, port);
        server->start_accept();
    }
}
