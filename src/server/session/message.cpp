#include <session.h>
#include <server.h>

#include <database.h>

#include <chrono>
#include <ctime>
#include <cstdio>
#include <atomic>

using nlohmann::json;
namespace asio = boost::asio;

namespace
{
    auto cached_world_conversation_id() -> asio::awaitable<i64>
    {
        static std::atomic<i64> cached{ 0 };
        auto val = cached.load(std::memory_order_relaxed);
        if(val > 0) co_return val;
        auto id = co_await database::get_world_conversation_id();
        cached.store(id, std::memory_order_relaxed);
        co_return id;
    }
} // namespace

auto Session::handle_send_msg(std::string payload) -> asio::awaitable<void>
{
    if(!authenticated_) {
        std::println("SEND_MSG from unauthenticated session ignored");
        co_return;
    }

    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        auto const err = make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
        auto msg = protocol::make_line("ERROR", err);
        send_text(std::move(msg));
        co_return;
    }

    if(!j.contains("content")) {
        auto const err = make_error_payload("INVALID_PARAM", "缺少 content 字段");
        auto msg = protocol::make_line("ERROR", err);
        send_text(std::move(msg));
        co_return;
    }

    auto const world_id = co_await cached_world_conversation_id();
    auto conversation_id = i64{};
    if(j.contains("conversationId")) {
        auto const conv_str = j.at("conversationId").get<std::string>();
        if(!conv_str.empty()) {
            conversation_id = std::stoll(conv_str);
        }
    }
    if(conversation_id <= 0) {
        conversation_id = world_id;
    }

    // 世界频道无需禁言校验，减少一次数据库访问
    if(conversation_id != world_id) {
        auto member = co_await database::get_conversation_member(conversation_id, user_id_);

        if(member) {
            auto const now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()
                                )
                                    .count();
            if(member->muted_until_ms > now_ms) {
                std::time_t tt = std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::time_point(
                        std::chrono::milliseconds(member->muted_until_ms)));
                std::tm tm = *std::localtime(&tt);
                char buf[32]{};
                std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_sec);

                auto const err =
                    make_error_payload("MUTED", std::string{ "你已被禁言至 " } + buf);
                auto msg = protocol::make_line("ERROR", err);
                send_text(std::move(msg));
                co_return;
            }
        }
    }

    auto const content = j.at("content").get<std::string>();
    auto const client_msg_id =
        j.contains("clientMsgId") ? j.at("clientMsgId").get<std::string>() : "";
    auto msg_type = std::string{ "TEXT" };
    if(j.contains("msgType")) {
        msg_type = j.at("msgType").get<std::string>();
    }

    database::StoredMessage stored{};

    try {
        // 检查 session 是否正在关闭
        if(closing_.load()) {
            co_return;
        }
        // 直接使用数据库写入消息，append_text_message 会生成 id 和 seq
        stored = co_await database::append_text_message(conversation_id, user_id_, content, msg_type);
    } catch(boost::system::system_error const& ex) {
        // Operation canceled / connection reset 是 session 关闭导致的正常情况，静默处理
        if(ex.code() == asio::error::operation_aborted ||
           ex.code() == asio::error::connection_reset ||
           ex.code() == asio::error::broken_pipe) {
            std::println("database write canceled (session closing)");
            co_return;
        }
        std::println("database write failed: {} ({})", ex.what(), ex.code().value());
        // socket 可能已关闭，检查后再发送错误
        if(socket_.is_open() && !closing_.load()) {
            auto const err = make_error_payload("SERVER_ERROR_DB", ex.what());
            auto msg = protocol::make_line("ERROR", err);
            send_text(std::move(msg));
        }
        co_return;
    } catch(std::exception const& ex) {
        std::println("database write failed: {}", ex.what());
        if(socket_.is_open() && !closing_.load()) {
            auto const err = make_error_payload("SERVER_ERROR_DB", ex.what());
            auto msg = protocol::make_line("ERROR", err);
            send_text(std::move(msg));
        }
        co_return;
    }

    // 检查 session 是否正在关闭或 socket 已关闭
    if(closing_.load() || !socket_.is_open()) {
        co_return;
    }

    json ack;
    ack["clientMsgId"] = client_msg_id;
    ack["serverMsgId"] = std::to_string(stored.id);
    ack["serverTimeMs"] = stored.server_time_ms;
    ack["seq"] = stored.seq;

    auto ack_msg = protocol::make_line("SEND_ACK", ack.dump());
    send_text(std::move(ack_msg));

    if(auto server = server_.lock()) {
        try {
            server->broadcast_world_message(stored, user_id_, content, display_name_);
        } catch(std::exception const& ex) {
            if(socket_.is_open()) {
                auto const err = make_error_payload("SERVER_ERROR_PUSH", ex.what());
                auto msg = protocol::make_line("ERROR", err);
                send_text(std::move(msg));
            }
        }
    }

    co_return;
}

auto Session::handle_history_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    // 使用非抛出版本的JSON解析
    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    }

    try {
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
            conversation_id = co_await cached_world_conversation_id();
        }

        // 直接从数据库加载历史消息
        std::vector<database::LoadedMessage> messages;
        if(after_seq > 0) {
            messages = co_await database::load_user_conversation_since(conversation_id, after_seq, limit);
        } else {
            messages = co_await database::load_user_conversation_history(conversation_id, before_seq, limit);
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

        co_return resp.dump();
    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}
