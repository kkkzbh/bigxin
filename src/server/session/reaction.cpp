#include <session.h>
#include <server.h>
#include <database.h>
#include <database/conversation.h>

#include <nlohmann/json.hpp>
#include <print>

using nlohmann::json;
namespace asio = boost::asio;

// 消息撤回请求处理
auto Session::handle_recall_msg_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    }

    try {
        if(!j.contains("conversationId") || !j.contains("serverMsgId")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要参数");
        }

        auto conversation_id_str = j.at("conversationId").get<std::string>();
        auto message_id_str = j.at("serverMsgId").get<std::string>();
        
        auto conversation_id = std::stoll(conversation_id_str);
        auto message_id = std::stoll(message_id_str);

        // 1. 查询消息信息
        auto conn_h = co_await database::acquire_connection();
        boost::mysql::results r_msg;
        co_await conn_h->async_execute(
            boost::mysql::with_params(
                "SELECT sender_id, conversation_id FROM messages WHERE id = {}",
                message_id),
            r_msg,
            asio::use_awaitable
        );

        if(r_msg.rows().empty()) {
            co_return make_error_payload("MESSAGE_NOT_FOUND", "消息不存在");
        }

        auto row = r_msg.rows().at(0);
        auto sender_id = row.at(0).as_int64();
        auto msg_conv_id = row.at(1).as_int64();

        // 2. 权限检查：必须是消息发送者，或者是群管理员/群主
        bool has_permission = false;
        if(sender_id == user_id_) {
            // 自己的消息可以撤回
            has_permission = true;
        } else {
            // 检查是否是群管理员或群主
            auto member = co_await database::get_conversation_member(conversation_id, user_id_);
            if(member && (member->role == "ADMIN" || member->role == "OWNER")) {
                has_permission = true;
            }
        }

        if(!has_permission) {
            co_return make_error_payload("NO_PERMISSION", "无权撤回该消息");
        }

        // 3. 执行撤回操作
        auto result = co_await database::recall_message(message_id, user_id_);

        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        // 4. 广播撤回通知
        if(auto server = server_.lock()) {
            server->broadcast_message_recalled(
                conversation_id,
                message_id,
                user_id_,
                display_name_
            );
        }

        // 5. 返回成功响应
        json resp;
        resp["ok"] = true;
        resp["conversationId"] = conversation_id_str;
        resp["serverMsgId"] = message_id_str;
        co_return resp.dump();

    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        std::println("handle_recall_msg_req error: {}", ex.what());
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

// 消息反应(点赞/踩)请求处理
auto Session::handle_msg_reaction_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    }

    try {
        if(!j.contains("conversationId") || !j.contains("serverMsgId") || !j.contains("reactionType")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要参数");
        }

        auto conversation_id_str = j.at("conversationId").get<std::string>();
        auto message_id_str = j.at("serverMsgId").get<std::string>();
        auto reaction_type = j.at("reactionType").get<std::string>();
        
        auto conversation_id = std::stoll(conversation_id_str);
        auto message_id = std::stoll(message_id_str);

        // 验证反应类型
        if(reaction_type != "LIKE" && reaction_type != "DISLIKE") {
            co_return make_error_payload("INVALID_PARAM", "无效的反应类型");
        }

        // 1. 检查消息是否存在
        auto conn_h = co_await database::acquire_connection();
        boost::mysql::results r_msg;
        co_await conn_h->async_execute(
            boost::mysql::with_params(
                "SELECT sender_id FROM messages WHERE id = {}",
                message_id),
            r_msg,
            asio::use_awaitable
        );

        if(r_msg.rows().empty()) {
            co_return make_error_payload("MESSAGE_NOT_FOUND", "消息不存在");
        }

        auto sender_id = r_msg.rows().at(0).at(0).as_int64();

        // 2. 不能给自己的消息点赞/踩
        if(sender_id == user_id_) {
            co_return make_error_payload("CANNOT_REACT_OWN", "不能给自己的消息点赞/踩");
        }

        // 3. 添加反应
        auto result = co_await database::add_message_reaction(message_id, user_id_, reaction_type);

        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        // 4. 广播反应更新
        if(auto server = server_.lock()) {
            server->broadcast_message_reaction(
                conversation_id,
                message_id,
                result.reactions
            );
        }

        // 5. 构造响应
        json resp;
        resp["ok"] = true;
        resp["conversationId"] = conversation_id_str;
        resp["serverMsgId"] = message_id_str;
        
        // 构造反应对象 {LIKE: [{userId, displayName}, ...], DISLIKE: [...]}
        json reactions_obj = json::object();
        reactions_obj["LIKE"] = json::array();
        reactions_obj["DISLIKE"] = json::array();

        for(auto const& reaction : result.reactions) {
            json user_obj;
            user_obj["userId"] = std::to_string(reaction.user_id);
            user_obj["displayName"] = reaction.display_name;
            reactions_obj[reaction.reaction_type].push_back(user_obj);
        }

        resp["reactions"] = reactions_obj;
        co_return resp.dump();

    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        std::println("handle_msg_reaction_req error: {}", ex.what());
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}

// 取消消息反应请求处理
auto Session::handle_msg_unreaction_req(std::string const& payload) -> asio::awaitable<std::string>
{
    if(!authenticated_) {
        co_return make_error_payload("NOT_AUTHENTICATED", "请先登录");
    }

    auto j = json::parse(payload, nullptr, false);
    if(j.is_discarded()) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    }

    try {
        if(!j.contains("conversationId") || !j.contains("serverMsgId") || !j.contains("reactionType")) {
            co_return make_error_payload("INVALID_PARAM", "缺少必要参数");
        }

        auto conversation_id_str = j.at("conversationId").get<std::string>();
        auto message_id_str = j.at("serverMsgId").get<std::string>();
        auto reaction_type = j.at("reactionType").get<std::string>();
        
        auto conversation_id = std::stoll(conversation_id_str);
        auto message_id = std::stoll(message_id_str);

        // 1. 移除反应
        auto result = co_await database::remove_message_reaction(message_id, user_id_, reaction_type);

        if(!result.ok) {
            co_return make_error_payload(result.error_code, result.error_msg);
        }

        // 2. 广播反应更新
        if(auto server = server_.lock()) {
            server->broadcast_message_reaction(
                conversation_id,
                message_id,
                result.reactions
            );
        }

        // 3. 构造响应
        json resp;
        resp["ok"] = true;
        resp["conversationId"] = conversation_id_str;
        resp["serverMsgId"] = message_id_str;
        
        // 构造反应对象
        json reactions_obj = json::object();
        reactions_obj["LIKE"] = json::array();
        reactions_obj["DISLIKE"] = json::array();

        for(auto const& reaction : result.reactions) {
            json user_obj;
            user_obj["userId"] = std::to_string(reaction.user_id);
            user_obj["displayName"] = reaction.display_name;
            reactions_obj[reaction.reaction_type].push_back(user_obj);
        }

        resp["reactions"] = reactions_obj;
        co_return resp.dump();

    } catch(json::parse_error const&) {
        co_return make_error_payload("INVALID_JSON", "请求 JSON 解析失败");
    } catch(std::exception const& ex) {
        std::println("handle_msg_unreaction_req error: {}", ex.what());
        co_return make_error_payload("SERVER_ERROR", ex.what());
    }
}
