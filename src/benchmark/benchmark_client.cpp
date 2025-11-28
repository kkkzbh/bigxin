#include "benchmark_client.h"

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <iostream>
#include <format>

namespace benchmark
{
    using namespace asio::experimental::awaitable_operators;

    BenchmarkClient::BenchmarkClient(asio::io_context& io)
        : io_{ io }
        , socket_{ io }
    {
    }

    BenchmarkClient::~BenchmarkClient()
    {
        close();
    }

    auto BenchmarkClient::async_connect(std::string const& host, std::uint16_t port)
        -> asio::awaitable<bool>
    {
        try {
            // 设置连接超时 5 秒
            asio::steady_timer timeout_timer{ io_ };
            timeout_timer.expires_after(std::chrono::seconds(5));

            tcp::resolver resolver{ io_ };

            // 使用 awaitable_operators 实现超时
            auto resolve_result = co_await (
                resolver.async_resolve(host, std::to_string(port), asio::use_awaitable)
                || timeout_timer.async_wait(asio::use_awaitable)
            );

            if(resolve_result.index() == 1) {
                // 超时
                co_return false;
            }

            auto endpoints = std::get<0>(resolve_result);

            // 重置超时计时器
            timeout_timer.expires_after(std::chrono::seconds(5));

            auto connect_result = co_await (
                asio::async_connect(socket_, endpoints, asio::use_awaitable)
                || timeout_timer.async_wait(asio::use_awaitable)
            );

            if(connect_result.index() == 1) {
                // 超时
                co_return false;
            }

            connected_ = true;
            co_return true;
        }
        catch(std::exception const&) {
            co_return false;
        }
    }

    auto BenchmarkClient::async_register(std::string const& account, std::string const& password)
        -> asio::awaitable<bool>
    {
        json payload;
        payload["account"] = account;
        payload["password"] = password;
        payload["confirmPassword"] = password;

        co_await send_command("REGISTER", payload);

        auto resp = co_await wait_for_response("REGISTER_RESP");
        auto ok = resp.contains("ok") && resp["ok"].get<bool>();
        co_return ok;
    }

    auto BenchmarkClient::async_login(std::string const& account, std::string const& password)
        -> asio::awaitable<std::string>
    {
        json payload;
        payload["account"] = account;
        payload["password"] = password;

        co_await send_command("LOGIN", payload);

        auto resp = co_await wait_for_response("LOGIN_RESP");

        if(resp.contains("ok") && resp["ok"].get<bool>()) {
            if(resp.contains("userId")) {
                user_id_ = resp["userId"].get<std::string>();
            }
            if(resp.contains("worldConversationId")) {
                world_conversation_id_ = resp["worldConversationId"].get<std::string>();
            }
            co_return user_id_;
        }

        user_id_.clear();
        world_conversation_id_.clear();
        co_return std::string{};
    }

    auto BenchmarkClient::async_create_group(
        std::string const& name,
        std::vector<std::string> const& member_ids
    ) -> asio::awaitable<std::string>
    {
        json payload;
        payload["name"] = name;
        payload["memberUserIds"] = member_ids;

        co_await send_command("CREATE_GROUP_REQ", payload);

        auto resp = co_await wait_for_response("CREATE_GROUP_RESP");

        if(resp.contains("ok") && resp["ok"].get<bool>()) {
            if(resp.contains("conversationId")) {
                co_return resp["conversationId"].get<std::string>();
            }
        }

        co_return "";
    }

    auto BenchmarkClient::generate_client_msg_id() -> std::string
    {
        auto count = msg_id_counter_.fetch_add(1);
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::format("{}_{}", now, count);
    }

    auto BenchmarkClient::send_message_fire_and_forget(
        std::string const& conversation_id,
        std::string const& content
    ) -> asio::awaitable<void>
    {
        auto client_msg_id = generate_client_msg_id();

        json payload;
        payload["conversationId"] = conversation_id;
        payload["conversationType"] = "GROUP";
        payload["senderId"] = user_id_;
        payload["clientMsgId"] = client_msg_id;
        payload["msgType"] = "TEXT";
        payload["content"] = content;

        // 记录待确认消息
        {
            std::lock_guard lock{ pending_mutex_ };
            pending_messages_[client_msg_id] = PendingMessage{
                .client_msg_id = client_msg_id,
                .send_time = std::chrono::steady_clock::now(),
                .status = MessageStatus::Pending
            };
        }

        ++ack_stats_.total_sent;

        // 发送后立即返回，不等待 ACK
        co_await send_command("SEND_MSG", payload);
    }

    auto BenchmarkClient::handle_send_ack(json const& payload) -> void
    {
        if(!payload.contains("clientMsgId")) {
            return;
        }

        auto client_msg_id = payload["clientMsgId"].get<std::string>();

        std::lock_guard lock{ pending_mutex_ };
        auto it = pending_messages_.find(client_msg_id);
        if(it != pending_messages_.end()) {
            it->second.status = MessageStatus::Confirmed;
            ++ack_stats_.ack_received;
            // 可以选择删除已确认的消息，或保留用于统计
            pending_messages_.erase(it);
        }
    }

    auto BenchmarkClient::start_background_reader() -> asio::awaitable<void>
    {
        while(is_connected()) {
            try {
                auto frame = co_await read_response();

                // 处理 SEND_ACK
                if(frame.command == "SEND_ACK") {
                    auto doc = json::parse(frame.payload);
                    handle_send_ack(doc);
                }
                // 处理 MSG_PUSH
                else if(frame.command == "MSG_PUSH") {
                    if(response_callback_) {
                        auto doc = json::parse(frame.payload);
                        response_callback_(frame.command, doc);
                    }
                }
                // 处理 PONG
                else if(frame.command == "PONG") {
                    // 心跳响应，忽略
                }
                // 其他消息放入队列（供 setup 阶段的同步等待使用）
                else {
                    pending_frames_.push_back(std::move(frame));
                }
            }
            catch(std::exception const&) {
                connected_ = false;
                break;
            }
        }
    }

    // 保留旧接口用于 setup 阶段
    auto BenchmarkClient::async_send_message(
        std::string const& conversation_id,
        std::string const& content
    ) -> asio::awaitable<bool>
    {
        json payload;
        payload["conversationId"] = conversation_id;
        payload["conversationType"] = "GROUP";
        payload["senderId"] = user_id_;
        payload["clientMsgId"] = std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count()
        );
        payload["msgType"] = "TEXT";
        payload["content"] = content;

        co_await send_command("SEND_MSG", payload);

        // 等待 SEND_ACK 确认
        auto resp = co_await wait_for_response("SEND_ACK");
        co_return resp.contains("serverMsgId");
    }

    auto BenchmarkClient::async_ping() -> asio::awaitable<void>
    {
        json payload = json::object();
        co_await send_command("PING", payload);
    }

    auto BenchmarkClient::close() -> void
    {
        if(connected_) {
            boost::system::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            socket_.close(ec);
            connected_ = false;
        }
    }

    auto BenchmarkClient::is_connected() const -> bool
    {
        return connected_ && socket_.is_open();
    }

    auto BenchmarkClient::set_response_callback(ResponseCallback callback) -> void
    {
        response_callback_ = std::move(callback);
    }

    auto BenchmarkClient::start_read_loop() -> asio::awaitable<void>
    {
        // 转发到新接口
        co_await start_background_reader();
    }

    auto BenchmarkClient::send_command(
        std::string const& command,
        json const& payload
    ) -> asio::awaitable<void>
    {
        auto payload_str = payload.dump();
        auto line = protocol::make_line(command, payload_str);

        co_await asio::async_write(
            socket_,
            asio::buffer(line),
            asio::use_awaitable
        );
    }

    auto BenchmarkClient::read_response() -> asio::awaitable<protocol::Frame>
    {
        auto n = co_await asio::async_read_until(
            socket_,
            read_buffer_,
            '\n',
            asio::use_awaitable
        );

        std::string line;
        line.resize(n);
        std::istream is{ &read_buffer_ };
        is.read(line.data(), static_cast<std::streamsize>(n));

        co_return protocol::parse_line(line);
    }

    auto BenchmarkClient::wait_for_response(std::string const& expected_command, int timeout_seconds)
        -> asio::awaitable<json>
    {
        // 先检查待处理队列
        for(auto it = pending_frames_.begin(); it != pending_frames_.end(); ++it) {
            if(it->command == expected_command) {
                auto frame = std::move(*it);
                pending_frames_.erase(it);
                co_return json::parse(frame.payload);
            }
        }

        // 设置超时
        asio::steady_timer timeout_timer{ io_ };
        timeout_timer.expires_after(std::chrono::seconds(timeout_seconds));

        // 持续读取直到获得期望的响应或超时
        while(true) {
            auto read_result = co_await (
                read_response()
                || timeout_timer.async_wait(asio::use_awaitable)
            );

            if(read_result.index() == 1) {
                // 超时，返回空 JSON
                co_return json{};
            }

            auto frame = std::get<0>(std::move(read_result));

            if(frame.command == expected_command) {
                co_return json::parse(frame.payload);
            }

            // 不是期望的命令，存入队列
            pending_frames_.push_back(std::move(frame));

            // 调用回调处理其他消息（如 MSG_PUSH）
            if(response_callback_ && !pending_frames_.empty()) {
                auto& last = pending_frames_.back();
                auto doc = json::parse(last.payload);
                if(doc.is_object()) {
                    response_callback_(last.command, doc);
                }
            }
        }
    }

} // namespace benchmark
