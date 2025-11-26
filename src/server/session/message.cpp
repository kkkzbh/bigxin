#include <session.h>
#include <server.h>

#include <database.h>

#include <chrono>
#include <ctime>
#include <cstdio>

using nlohmann::json;

auto Session::handle_send_msg(std::string payload) -> asio::awaitable<void>
{
    if(!authenticated_) {
        std::println("SEND_MSG from unauthenticated session ignored");
        co_return;
    }

    // 使用非抛出版本的JSON解析,减少异常开销
    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        auto const err = make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
        auto msg = protocol::make_line("ERROR", err);
        send_text(std::move(msg));
        co_return;
    }

    try {
        if(!j.contains("content")) {
            auto const err = make_error_payload("INVALID_PARAM", "缺少 content 字段");
            auto msg = protocol::make_line("ERROR", err);
            send_text(std::move(msg));
            co_return;
        }

        auto const world_id = co_await database::get_world_conversation_id();
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

        auto stored = co_await database::append_text_message(conversation_id, user_id_, content);

        json ack;
        ack["clientMsgId"] = client_msg_id;
        ack["serverMsgId"] = std::to_string(stored.id);
        ack["serverTimeMs"] = stored.server_time_ms;
        ack["seq"] = stored.seq;

        auto ack_msg = protocol::make_line("SEND_ACK", ack.dump());
        send_text(std::move(ack_msg));

        if(server_ != nullptr) {
            server_->broadcast_world_message(stored, user_id_, content, display_name_);
        }
    } catch(std::exception const& ex) {
        auto const err = make_error_payload("SERVER_ERROR", ex.what());
        auto msg = protocol::make_line("ERROR", err);
        send_text(std::move(msg));
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
            conversation_id = co_await database::get_world_conversation_id();
        }

        std::vector<database::LoadedMessage> messages;
        if(after_seq > 0) {
            messages = co_await database::load_user_conversation_since(conversation_id, after_seq, limit);
        } else {
            messages =
                co_await database::load_user_conversation_history(conversation_id, before_seq, limit);
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
