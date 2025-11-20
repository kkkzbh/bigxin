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
                    } else if(frame.command == "FRIEND_LIST_REQ") {
                        auto payload = handle_friend_list_req(frame.payload);
                        auto msg = protocol::make_line("FRIEND_LIST_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "FRIEND_SEARCH_REQ") {
                        auto payload = handle_friend_search_req(frame.payload);
                        auto msg = protocol::make_line("FRIEND_SEARCH_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "FRIEND_ADD_REQ") {
                        auto payload = handle_friend_add_req(frame.payload);
                        auto msg = protocol::make_line("FRIEND_ADD_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "FRIEND_REQ_LIST_REQ") {
                        auto payload = handle_friend_req_list_req(frame.payload);
                        auto msg = protocol::make_line("FRIEND_REQ_LIST_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "FRIEND_ACCEPT_REQ") {
                        auto payload = handle_friend_accept_req(frame.payload);
                        auto msg = protocol::make_line("FRIEND_ACCEPT_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "CREATE_GROUP_REQ") {
                        auto payload = handle_create_group_req(frame.payload);
                        auto msg = protocol::make_line("CREATE_GROUP_RESP", payload);
                        send_text(std::move(msg));
                    } else if(frame.command == "OPEN_SINGLE_CONV_REQ") {
                        auto payload = handle_open_single_conv_req(frame.payload);
                        auto msg = protocol::make_line("OPEN_SINGLE_CONV_RESP", payload);
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

        /// \brief 处理好友列表请求，返回 FRIEND_LIST_RESP 的 JSON 串。
        /// \param payload FRIEND_LIST_REQ 的 JSON 文本。
        auto handle_friend_list_req(std::string const& payload) -> std::string;

        /// \brief 处理按账号搜索好友的请求，返回 FRIEND_SEARCH_RESP 的 JSON 串。
        /// \param payload FRIEND_SEARCH_REQ 的 JSON 文本。
        auto handle_friend_search_req(std::string const& payload) -> std::string;

        /// \brief 处理创建好友申请的请求，返回 FRIEND_ADD_RESP 的 JSON 串。
        /// \param payload FRIEND_ADD_REQ 的 JSON 文本。
        auto handle_friend_add_req(std::string const& payload) -> std::string;

        /// \brief 处理“新的朋友”列表请求，返回 FRIEND_REQ_LIST_RESP 的 JSON 串。
        /// \param payload FRIEND_REQ_LIST_REQ 的 JSON 文本。
        auto handle_friend_req_list_req(std::string const& payload) -> std::string;

        /// \brief 处理同意好友申请的请求，返回 FRIEND_ACCEPT_RESP 的 JSON 串。
        /// \param payload FRIEND_ACCEPT_REQ 的 JSON 文本。
        auto handle_friend_accept_req(std::string const& payload) -> std::string;

        /// \brief 处理创建群聊的请求，返回 CREATE_GROUP_RESP 的 JSON 串。
        /// \param payload CREATE_GROUP_REQ 的 JSON 文本。
        auto handle_create_group_req(std::string const& payload) -> std::string;

        /// \brief 处理打开单聊会话的请求，返回 OPEN_SINGLE_CONV_RESP 的 JSON 串。
        /// \param payload OPEN_SINGLE_CONV_REQ 的 JSON 文本。
        auto handle_open_single_conv_req(std::string const& payload) -> std::string;

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

        /// \brief 主动向指定用户推送一份最新的“新的朋友”列表。
        /// \details 用 FRIEND_REQ_LIST_RESP 的形式下发，复用现有前端处理逻辑。
        auto send_friend_request_list_to(i64 target_user_id) -> void
        {
            using json = nlohmann::json;

            if(target_user_id <= 0) {
                return;
            }

            try {
                auto const requests = database::load_incoming_friend_requests(target_user_id);

                json resp;
                resp["ok"] = true;

                json items = json::array();
                for(auto const& r : requests) {
                    json obj;
                    obj["requestId"] = std::to_string(r.id);
                    obj["fromUserId"] = std::to_string(r.from_user_id);
                    obj["account"] = r.account;
                    obj["displayName"] = Session::normalize_whitespace(r.display_name);
                    obj["status"] = r.status;
                    obj["helloMsg"] = r.hello_msg;
                    items.push_back(std::move(obj));
                }

                resp["requests"] = std::move(items);
                auto const line =
                    protocol::make_line("FRIEND_REQ_LIST_RESP", resp.dump());

                for(auto const& s : sessions_) {
                    if(!s) {
                        continue;
                    }
                    if(!s->authenticated_) {
                        continue;
                    }
                    if(s->user_id_ != target_user_id) {
                        continue;
                    }
                    s->send_text(line);
                }
            } catch(std::exception const&) {
                // 失败时静默忽略，等待客户端下次主动拉取。
            }
        }

        /// \brief 主动向指定用户推送一份最新的好友列表。
        /// \details 复用 FRIEND_LIST_RESP 结构，便于前端直接重建模型。
        auto send_friend_list_to(i64 target_user_id) -> void
        {
            using json = nlohmann::json;

            if(target_user_id <= 0) {
                return;
            }

            try {
                auto const friends = database::load_user_friends(target_user_id);

                json resp;
                resp["ok"] = true;

                json items = json::array();
                        for(auto const& f : friends) {
                            json u;
                            u["userId"] = std::to_string(f.id);
                            u["account"] = f.account;
                            u["displayName"] = Session::normalize_whitespace(f.display_name);
                            u["region"] = "";
                            u["signature"] = "";
                            items.push_back(std::move(u));
                        }

                resp["friends"] = std::move(items);
                auto const line = protocol::make_line("FRIEND_LIST_RESP", resp.dump());

                for(auto const& s : sessions_) {
                    if(!s || !s->is_authenticated()) {
                        continue;
                    }
                    if(s->user_id() != target_user_id) {
                        continue;
                    }
                    s->send_text(line);
                }
            } catch(std::exception const&) {
                // 忽略推送失败。
            }
        }

        /// \brief 主动下发会话列表给指定用户，结构同 CONV_LIST_RESP。
        auto send_conv_list_to(i64 target_user_id) -> void
        {
            using json = nlohmann::json;

            if(target_user_id <= 0) {
                return;
            }

            try {
                auto const conversations = database::load_user_conversations(target_user_id);

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
                auto const line = protocol::make_line("CONV_LIST_RESP", resp.dump());

                for(auto const& s : sessions_) {
                    if(!s || !s->is_authenticated()) {
                        continue;
                    }
                    if(s->user_id() != target_user_id) {
                        continue;
                    }
                    s->send_text(line);
                }
            } catch(std::exception const&) {
                // 忽略推送失败。
            }
        }

        /// \brief 将系统消息以 MSG_PUSH 形式广播给指定会话成员。
        auto broadcast_system_message(
            i64 conversation_id,
            database::StoredMessage const& stored,
            std::string const& content
        ) -> void
        {
            using json = nlohmann::json;

            json push;
            push["conversationId"] = std::to_string(conversation_id);
            push["conversationType"] = "GROUP";
            push["serverMsgId"] = std::to_string(stored.id);
            push["senderId"] = "0";
            push["senderDisplayName"] = "";
            push["msgType"] = "TEXT";
            push["serverTimeMs"] = stored.server_time_ms;
            push["seq"] = stored.seq;
            push["content"] = content;

            auto const line = protocol::make_line("MSG_PUSH", push.dump());

            std::vector<i64> member_ids;
            try {
                auto conn = database::make_connection();
                pqxx::work tx{ conn };
                auto const query =
                    "SELECT user_id FROM conversation_members WHERE conversation_id = "
                    + tx.quote(conversation_id);
                auto rows = tx.exec(query);
                member_ids.reserve(rows.size());
                for(auto const& row : rows) {
                    member_ids.push_back(row[0].as<i64>());
                }
                tx.commit();
            } catch(std::exception const&) {
                member_ids.clear(); // broadcast to all authenticated if query failed.
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
            std::string sender_name;
            try {
                auto conn = database::make_connection();
                pqxx::work tx{ conn };
                auto const query =
                    "SELECT c.type, u.display_name FROM conversations c "
                    "JOIN users u ON u.id = " + tx.quote(sender_id) + " "
                    "WHERE c.id = " + tx.quote(stored.conversation_id) + " LIMIT 1";
                auto rows = tx.exec(query);
                if(!rows.empty()) {
                    conv_type = rows[0][0].as<std::string>();
                    sender_name = rows[0][1].as<std::string>();
                }
                tx.commit();
            } catch(std::exception const&) {
                // 保底使用 GROUP。
            }
            push["conversationType"] = conv_type;
            push["serverMsgId"] = std::to_string(stored.id);
            push["senderId"] = std::to_string(sender_id);
            push["senderDisplayName"] = sender_name;
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
            m["senderDisplayName"] = msg.sender_display_name;
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

inline auto server::Session::handle_friend_list_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            // 目前好友列表不支持过滤参数，仅校验 JSON 格式。
            (void)json::parse(payload);
        }

        auto const friends = database::load_user_friends(user_id_);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& f : friends) {
            json u;
            u["userId"] = std::to_string(f.id);
            u["account"] = f.account;
            u["displayName"] = f.display_name;
            // 当前没有地区 / 个性签名字段，预留为空串。
            u["region"] = "";
            u["signature"] = "";
            items.push_back(std::move(u));
        }

        resp["friends"] = std::move(items);
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_friend_search_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("account")) {
            return make_error_payload("INVALID_PARAM", "缺少 account 字段");
        }

        auto const account = j.at("account").get<std::string>();

        auto const result = database::search_friend_by_account(user_id_, account);
        if(!result.ok) {
            return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;

        json user;
        user["userId"] = std::to_string(result.user.id);
        user["account"] = result.user.account;
        user["displayName"] = result.user.display_name;
        user["region"] = "";
        user["signature"] = "";

        resp["user"] = std::move(user);
        resp["isFriend"] = result.is_friend;
        resp["isSelf"] = result.is_self;
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_friend_add_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("peerUserId")) {
            return make_error_payload("INVALID_PARAM", "缺少 peerUserId 字段");
        }

        auto const peer_str = j.at("peerUserId").get<std::string>();
        auto peer_id = i64{};
        try {
            peer_id = std::stoll(peer_str);
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }
        if(peer_id <= 0) {
            return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }

        auto const source =
            j.contains("source") ? j.at("source").get<std::string>() : std::string{ "search_account" };
        auto const hello_msg =
            j.contains("helloMsg") ? j.at("helloMsg").get<std::string>() : std::string{};

        auto const result =
            database::create_friend_request(user_id_, peer_id, source, hello_msg);
        if(!result.ok) {
            return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;
        resp["requestId"] = std::to_string(result.request_id);

        // 通知对端用户刷新“新的朋友”列表。
        if(server_ != nullptr) {
            server_->send_friend_request_list_to(peer_id);
        }

        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_friend_req_list_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        if(!payload.empty() && payload != "{}") {
            (void)json::parse(payload);
        }

        auto const requests = database::load_incoming_friend_requests(user_id_);

        json resp;
        resp["ok"] = true;

        json items = json::array();
        for(auto const& r : requests) {
            json obj;
            obj["requestId"] = std::to_string(r.id);
            obj["fromUserId"] = std::to_string(r.from_user_id);
            obj["account"] = r.account;
            obj["displayName"] = r.display_name;
            obj["status"] = r.status;
            obj["helloMsg"] = r.hello_msg;
            items.push_back(std::move(obj));
        }

        resp["requests"] = std::move(items);
        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_friend_accept_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("requestId")) {
            return make_error_payload("INVALID_PARAM", "缺少 requestId 字段");
        }

        auto const id_str = j.at("requestId").get<std::string>();
        auto request_id = i64{};
        try {
            request_id = std::stoll(id_str);
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "requestId 非法");
        }
        if(request_id <= 0) {
            return make_error_payload("INVALID_PARAM", "requestId 非法");
        }

        auto const result = database::accept_friend_request(request_id, user_id_);
        if(!result.ok) {
            return make_error_payload(result.error_code, result.error_msg);
        }

        json resp;
        resp["ok"] = true;

        json f;
        f["userId"] = std::to_string(result.friend_user.id);
        f["account"] = result.friend_user.account;
        f["displayName"] = result.friend_user.display_name;
        resp["friend"] = std::move(f);

        if(result.conversation_id > 0) {
            resp["conversationId"] = std::to_string(result.conversation_id);
            resp["conversationType"] = "SINGLE";
        } else {
            resp["conversationId"] = "";
        }

        // 同步双方的好友列表 / “新的朋友”列表 / 会话列表，确保前端及时刷新。
        if(server_ != nullptr) {
            server_->send_friend_list_to(user_id_);
            server_->send_friend_list_to(result.friend_user.id);
            server_->send_friend_request_list_to(user_id_);
            server_->send_friend_request_list_to(result.friend_user.id);
            if(result.conversation_id > 0) {
                server_->send_conv_list_to(user_id_);
                server_->send_conv_list_to(result.friend_user.id);
            }
        }

        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_create_group_req(std::string const& payload) -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("memberUserIds") || !j.at("memberUserIds").is_array()) {
            return make_error_payload("INVALID_PARAM", "缺少 memberUserIds 数组");
        }

        auto const arr = j.at("memberUserIds");
        std::vector<i64> members;
        members.reserve(arr.size());
        for(auto const& item : arr) {
            auto const str = item.get<std::string>();
            auto id = i64{};
            try {
                id = std::stoll(str);
            } catch(std::exception const&) {
                return make_error_payload("INVALID_PARAM", "memberUserIds 中存在非法 ID");
            }
            if(id > 0) {
                members.push_back(id);
            }
        }

        // 至少要两位好友，加上创建者总计>=3
        if(members.size() < 2) {
            return make_error_payload("INVALID_PARAM", "群成员不足");
        }

        auto name = std::string{};
        if(j.contains("name")) {
            name = j.at("name").get<std::string>();
            auto trim = [](std::string& s) {
                auto const not_space = [](unsigned char ch) { return !std::isspace(ch); };
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
                s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
            };
            trim(name);
        }

        auto const conv_id = database::create_group_conversation(user_id_, members, name);

        // 取实际群名
        std::string conv_name{ "群聊" };
        {
            auto conn = database::make_connection();
            pqxx::work tx{ conn };
            auto const q =
                "SELECT name FROM conversations WHERE id = " + tx.quote(conv_id) + " LIMIT 1";
            auto rows = tx.exec(q);
            if(!rows.empty()) {
                conv_name = rows[0][0].as<std::string>();
            }
            tx.commit();
        }

        // 写首条系统消息
        auto const sys_content = std::string{ "你们创建了群聊：" } + conv_name;
        auto const stored = database::append_text_message(conv_id, 0, sys_content);

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["conversationType"] = "GROUP";
        resp["title"] = conv_name;
        resp["memberCount"] = static_cast<i64>(members.size() + 1);

        // 推送会话列表 & 系统消息给全体成员
        if(server_ != nullptr) {
            // 推送 creator + members
            server_->send_conv_list_to(user_id_);
            for(auto const uid : members) {
                server_->send_conv_list_to(uid);
            }
            server_->broadcast_system_message(conv_id, stored, sys_content);
        }

        return resp.dump();
    } catch(json::parse_error const&) {
        return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        return make_error_payload("SERVER_ERROR", ex.what());
    }
}

inline auto server::Session::handle_open_single_conv_req(std::string const& payload)
    -> std::string
{
    using json = nlohmann::json;

    if(!authenticated_) {
        return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    try {
        auto j = payload.empty() ? json::object() : json::parse(payload);

        if(!j.contains("peerUserId")) {
            return make_error_payload("INVALID_PARAM", "缺少 peerUserId 字段");
        }

        auto const peer_str = j.at("peerUserId").get<std::string>();
        auto peer_id = i64{};
        try {
            peer_id = std::stoll(peer_str);
        } catch(std::exception const&) {
            return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }
        if(peer_id <= 0) {
            return make_error_payload("INVALID_PARAM", "peerUserId 非法");
        }
        if(peer_id == user_id_) {
            return make_error_payload("INVALID_PARAM", "不能与自己建立单聊");
        }

        if(!database::is_friend(user_id_, peer_id)) {
            return make_error_payload("NOT_FRIEND", "对方还不是你的好友");
        }

        auto const conv_id = database::get_or_create_single_conversation(user_id_, peer_id);

        json resp;
        resp["ok"] = true;
        resp["conversationId"] = std::to_string(conv_id);
        resp["conversationType"] = "SINGLE";
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
