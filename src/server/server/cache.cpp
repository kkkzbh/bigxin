/**
 * @file
 * @brief 会话缓存的加载、失效与过期清理逻辑。
 *
 * 负责维护 conversations 的内存缓存，缓存会话类型与成员列表，
 * 以减少对数据库的访问频率。所有对缓存的访问通过 `cache_mutex_`
 * 保护，保证在多线程环境下的正确性。
 */
#include <server.h>
#include <database.h>
#include <database/conversation.h>

#include <optional>
#include <print>
#include <algorithm>

/**
 * @brief 获取指定会话的缓存信息。
 *
 * 优先从内存缓存中读取会话类型与成员列表；若缓存未命中，则回源数据库，
 * 将结果写入缓存并返回。若会话 ID 非法或加载失败，则返回空。
 *
 * @param conversation_id 会话 ID，小于等于 0 时直接返回空。
 * @return 对应会话的缓存对象；当不存在或加载失败时为 `std::nullopt`。
 */
auto Server::get_conversation_cache(i64 conversation_id) -> std::optional<ConversationCache>
{
    if(conversation_id <= 0) {
        return std::nullopt;
    }

    // 先尝试从缓存获取
    {
        std::lock_guard lock{ cache_mutex_ };
        auto it = conv_cache_.find(conversation_id);
        if(it != conv_cache_.end()) {
            // 更新最后访问时间
            it->second.last_access = std::chrono::steady_clock::now();
            return it->second;
        }
    }

    // 暂不从数据库回源，直接返回空，调用方将退化为广播给所有在线会话
    return std::nullopt;
}

/**
 * @brief 使指定会话的缓存条目失效。
 *
 * 从 `conv_cache_` 中移除给定会话 ID 对应的缓存记录。
 *
 * @param conversation_id 目标会话 ID，小于等于 0 时不做处理。
 */
auto Server::invalidate_conversation_cache(i64 conversation_id) -> void
{
    if(conversation_id <= 0) {
        return;
    }

    std::lock_guard lock{ cache_mutex_ };
    conv_cache_.erase(conversation_id);
}

/**
 * @brief 清理超时未访问的会话缓存。
 *
 * 遍历 `conv_cache_`，删除 `last_access` 超过 `CACHE_EXPIRE_DURATION`
 * 的缓存条目，用于限制缓存大小和过期数据驻留时间。
 */
auto Server::cleanup_expired_cache() -> void
{
    std::lock_guard lock{ cache_mutex_ };
    auto const now = std::chrono::steady_clock::now();

    std::erase_if(conv_cache_, [&](auto const& pair) {
        auto const age = now - pair.second.last_access;
        return age > CACHE_EXPIRE_DURATION;
    });

    std::erase_if(member_cache_, [&](auto const& pair) {
        auto const age = now - pair.second.last_access;
        return age > CACHE_EXPIRE_DURATION;
    });
}

auto Server::get_member_list_cache(i64 conversation_id) -> std::optional<MemberListCache>
{
    if(conversation_id <= 0) {
        return std::nullopt;
    }

    std::lock_guard lock{ cache_mutex_ };
    auto it = member_cache_.find(conversation_id);
    if(it == member_cache_.end()) {
        return std::nullopt;
    }
    it->second.last_access = std::chrono::steady_clock::now();
    return it->second;
}

auto Server::set_member_list_cache(i64 conversation_id, std::vector<database::MemberInfo> members) -> void
{
    if(conversation_id <= 0) {
        return;
    }
    MemberListCache cache{
        .members = std::move(members),
        .last_access = std::chrono::steady_clock::now()
    };
    std::lock_guard lock{ cache_mutex_ };
    member_cache_[conversation_id] = std::move(cache);
}

auto Server::invalidate_member_list_cache(i64 conversation_id) -> void
{
    if(conversation_id <= 0) {
        return;
    }
    std::lock_guard lock{ cache_mutex_ };
    member_cache_.erase(conversation_id);
}
