#include <redis_client.h>

#include <boost/redis/connection.hpp>
#include <boost/redis/request.hpp>
#include <boost/redis/response.hpp>
#include <boost/redis/ignore.hpp>
#include <boost/redis/error.hpp>

#include <nlohmann/json.hpp>

#define BOOST_REDIS_SEPARATE_COMPILATION
#include <boost/redis/src.hpp>

#include <mutex>
#include <stdexcept>
#include <optional>
#include <print>
#include <algorithm>
#include <system_error>
#include <future>

namespace asio = boost::asio;
namespace br = boost::redis;

namespace redis
{
    namespace
    {
        struct PooledConnection
        {
            std::shared_ptr<br::connection> conn;
            std::shared_future<void> run_future;
        };

        struct PoolState
        {
            Config cfg{};
            asio::any_io_executor exec{};
            std::size_t created{ 0 };
            bool initialized{ false };
            std::vector<std::shared_ptr<PooledConnection>> idle;
            std::vector<std::shared_ptr<PooledConnection>> all; // 跟踪全部连接,用于优雅关闭
            std::mutex mutex;
        };

        PoolState& state()
        {
            // 使用堆上单例,避免进程退出时的析构次序问题。
            static PoolState* s = new PoolState;
            return *s;
        }

        auto start_run(std::shared_ptr<br::connection> const& conn, br::config cfg) -> std::shared_future<void>
        {
            // promise 用 shared_future,方便 shutdown 等待。
            auto prom = std::make_shared<std::promise<void>>();
            auto fut = prom->get_future().share();

            asio::co_spawn(
                conn->get_executor(),
                [conn, cfg, prom]() -> asio::awaitable<void> {
                    boost::system::error_code ec;
                    co_await conn->async_run(cfg, asio::redirect_error(asio::use_awaitable, ec));
                    prom->set_value();
                },
                asio::detached
            );

            return fut;
        }

        auto make_connection() -> asio::awaitable<std::shared_ptr<PooledConnection>>
        {
            auto& st = state();
            auto pc = std::make_shared<PooledConnection>();
            pc->conn = std::make_shared<br::connection>(st.exec);

            br::config cfg;
            cfg.addr.host = st.cfg.host;
            cfg.addr.port = st.cfg.port;
            cfg.health_check_interval = std::chrono::seconds{ 15 };

            pc->run_future = start_run(pc->conn, cfg);

            // 简单握手确认连接可用
            br::request req;
            req.push("PING");
            br::response<std::string> resp;
            co_await pc->conn->async_exec(req, resp, asio::use_awaitable);

            {
                std::lock_guard lock{ st.mutex };
                st.all.push_back(pc);
            }

            co_return pc;
        }

        auto acquire_connection() -> asio::awaitable<std::shared_ptr<PooledConnection>>
        {
            auto& st = state();
            if(!st.initialized) {
                throw std::runtime_error("redis pool not initialized");
            }

            {
                std::lock_guard lock{ st.mutex };
                if(!st.idle.empty()) {
                    auto c = st.idle.back();
                    st.idle.pop_back();
                    co_return c;
                }
                if(st.created < st.cfg.pool_size) {
                    ++st.created;
                    // 创建新连接，后续 return
                }
            }

            co_return co_await make_connection();
        }

        auto release_connection(std::shared_ptr<PooledConnection> conn) -> void
        {
            auto& st = state();
            if(!st.initialized || !conn) return;
            std::lock_guard lock{ st.mutex };
            if(st.idle.size() < st.cfg.pool_size) {
                st.idle.push_back(std::move(conn));
            }
        }

        struct ConnectionHandle
        {
            std::shared_ptr<PooledConnection> pooled;
            ConnectionHandle() = default;
            explicit ConnectionHandle(std::shared_ptr<PooledConnection> c) : pooled(std::move(c)) {}
            ConnectionHandle(ConnectionHandle&&) noexcept = default;
            ConnectionHandle& operator=(ConnectionHandle&&) noexcept = default;
            ~ConnectionHandle() { release_connection(std::move(pooled)); }
            br::connection& operator*() const { return *pooled->conn; }
            br::connection* operator->() const { return pooled->conn.get(); }
            explicit operator bool() const { return static_cast<bool>(pooled); }
        };

        auto acquire_handle() -> asio::awaitable<ConnectionHandle>
        {
            auto c = co_await acquire_connection();
            co_return ConnectionHandle{ std::move(c) };
        }

        auto messages_key(i64 conversation_id) -> std::string
        {
            return "chat:conv:" + std::to_string(conversation_id) + ":msgs";
        }

        auto seq_key(i64 conversation_id) -> std::string
        {
            return "chat:conv:" + std::to_string(conversation_id) + ":seq";
        }

        auto parse_messages(std::vector<std::string> const& raw)
            -> std::vector<database::LoadedMessage>
        {
            std::vector<database::LoadedMessage> out;
            out.reserve(raw.size());
            for(auto const& item : raw) {
                try {
                    auto j = nlohmann::json::parse(item);
                    database::LoadedMessage msg{};
                    msg.id = j.value("id", 0LL);
                    msg.conversation_id = j.value("conversationId", 0LL);
                    msg.sender_id = j.value("senderId", 0LL);
                    msg.sender_display_name = j.value("senderDisplayName", std::string{});
                    msg.seq = j.value("seq", 0LL);
                    msg.msg_type = j.value("msgType", std::string{ "TEXT" });
                    msg.content = j.value("content", std::string{});
                    msg.server_time_ms = j.value("serverTimeMs", 0LL);
                    out.push_back(std::move(msg));
                } catch(std::exception const& ex) {
                    std::println("redis parse message failed: {}", ex.what());
                }
            }
            return out;
        }
    } // namespace

    auto init_pool(asio::any_io_executor exec, Config cfg) -> void
    {
        auto& st = state();
        if(st.initialized) return;
        st.exec = exec;
        st.cfg = std::move(cfg);
        st.created = 0;
        st.initialized = true;
    }

    auto shutdown_pool() -> void
    {
        auto& st = state();
        std::vector<std::shared_ptr<PooledConnection>> conns;
        {
            std::lock_guard lock{ st.mutex };
            conns.swap(st.all);
            st.idle.clear();
            st.initialized = false;
        }

        // 先取消,再等待 async_run 协程自然退出。
        for(auto& pc : conns) {
            if(pc && pc->conn) {
                pc->conn->cancel(br::operation::all);
            }
        }
        for(auto& pc : conns) {
            if(pc && pc->run_future.valid()) {
                pc->run_future.wait_for(std::chrono::seconds{ 1 });
            }
        }
    }

    auto next_message_id() -> asio::awaitable<i64>
    {
        auto h = co_await acquire_handle();
        br::request req;
        req.push("INCR", "chat:global:msg:id");
        br::response<i64> resp;
        co_await h->async_exec(req, resp);
        auto result = std::get<0>(resp);
        if(!result) {
            throw std::runtime_error(result.error().diagnostic);
        }
        co_return result.value();
    }

    auto next_conversation_seq(i64 conversation_id) -> asio::awaitable<i64>
    {
        auto h = co_await acquire_handle();
        br::request req;
        req.push("INCR", seq_key(conversation_id));
        br::response<i64> resp;
        co_await h->async_exec(req, resp);
        auto result = std::get<0>(resp);
        if(!result) {
            throw std::runtime_error(result.error().diagnostic);
        }
        co_return result.value();
    }

    auto write_message(
        database::StoredMessage const& stored,
        i64 sender_id,
        std::string const& sender_display_name,
        std::string const& content
    ) -> asio::awaitable<void>
    {
        auto h = co_await acquire_handle();
        nlohmann::json j;
        j["id"] = stored.id;
        j["conversationId"] = stored.conversation_id;
        j["senderId"] = sender_id;
        j["senderDisplayName"] = sender_display_name;
        j["seq"] = stored.seq;
        j["msgType"] = stored.msg_type.empty() ? "TEXT" : stored.msg_type;
        j["content"] = content;
        j["serverTimeMs"] = stored.server_time_ms;

        auto payload = j.dump();

        br::request req;
        req.push("ZADD", messages_key(stored.conversation_id), stored.seq, payload);
        req.push("PUBLISH", "chat:conv:" + std::to_string(stored.conversation_id) + ":channel", payload);

        co_await h->async_exec(req, br::ignore);
        co_return;
    }

    auto load_history(
        i64 conversation_id,
        i64 after_seq,
        i64 before_seq,
        i64 limit
    ) -> asio::awaitable<std::vector<database::LoadedMessage>>
    {
        if(limit <= 0) limit = 50;
        auto h = co_await acquire_handle();
        br::request req;
        if(after_seq > 0) {
            req.push("ZRANGEBYSCORE",
                messages_key(conversation_id),
                "(" + std::to_string(after_seq),
                "+inf",
                "LIMIT",
                "0",
                std::to_string(limit));
        } else if(before_seq > 0) {
            // 向前翻页：获取小于 before_seq 的 limit 条，倒序再反转。
            req.push("ZREVRANGEBYSCORE",
                messages_key(conversation_id),
                std::to_string(before_seq - 1),
                "-inf",
                "LIMIT",
                "0",
                std::to_string(limit));
        } else {
            req.push("ZREVRANGE",
                messages_key(conversation_id),
                "0",
                std::to_string(limit - 1));
        }

        br::response<std::vector<std::string>> resp;
        co_await h->async_exec(req, resp);
        auto result = std::get<0>(resp);
        if(!result) {
            co_return std::vector<database::LoadedMessage>{};
        }
        auto raw = std::move(result.value());
        if(after_seq > 0) {
            co_return parse_messages(raw);
        }
        // 对于 ZREVRANGE / ZREVRANGEBYSCORE 需要正序返回
        std::reverse(raw.begin(), raw.end());
        co_return parse_messages(raw);
    }
} // namespace redis
