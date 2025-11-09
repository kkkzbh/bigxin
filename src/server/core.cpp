module;

#include <boost/asio.hpp>

export module server.core;

import std;
import io.core;
import protocol;
import utility;

namespace asio = boost::asio;
using util::use_awaitable_noexcept;
// 使用 string_view 字面量与避免窄化的初始化风格
using namespace std::string_view_literals;
auto static constexpr DEFAULT_ADDR = "0.0.0.0"sv;
auto static constexpr DEFAULT_PORT = std::uint16_t(7777);
auto static constexpr DEFAULT_MAX_PENDING = std::size_t{ 256 };
auto static constexpr DEFAULT_MAX_LINE = std::size_t{ 1024 };

using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
using asio::async_write;
using asio::async_read_until;
using asio::dynamic_string_buffer;
using asio::buffer;
using asio::make_strand;
using asio::strand;
    

struct ServerConfig
{
    std::string addr{ DEFAULT_ADDR };
    std::uint16_t port{ DEFAULT_PORT };
    std::size_t threads{ 8 };
    std::size_t max_pending_send{ DEFAULT_MAX_PENDING };
    std::size_t max_line{ DEFAULT_MAX_LINE };
};

struct Session;

// 前置声明一个自由函数，避免 Hub <-> Session 的循环依赖
auto session_send_line(std::shared_ptr<Session> const& s, std::string const& msg) -> void;

struct Hub : std::enable_shared_from_this<Hub>
{
    explicit Hub(asio::any_io_executor ex) : strand_(make_strand(std::move(ex))) {}

    auto join(std::shared_ptr<Session> const& s) -> void
    {
        asio::dispatch (
            strand_,
            [self = shared_from_this(), wp = std::weak_ptr{ s }] mutable {
                self->sessions_.push_back(std::move(wp));
            }
        );
    }

    auto leave(std::shared_ptr<Session> const& s) -> void
    {
        asio::dispatch (
            strand_,
            [self = shared_from_this(), sp = std::weak_ptr{ s }] mutable {
                auto is_same = [&sp](std::weak_ptr<Session> const& w) {
                    return not w.owner_before(sp) and not sp.owner_before(w);
                };
                std::erase_if(self->sessions_, is_same);
            }
        );
    }

    auto broadcast(std::string msg) -> void
    {
        asio::dispatch (
            strand_,
            [self = shared_from_this(), msg = std::move(msg)] mutable {
                // 清理失效，同时广播
                auto alive = std::vector<std::weak_ptr<Session>>{};
                alive.reserve(self->sessions_.size());
                for (auto& w : self->sessions_) {
                    if (auto s = w.lock()) {
                        session_send_line(s, msg);
                        alive.push_back(w);
                    }
                }
                self->sessions_.swap(alive);
            }
        );
    }

private:
    strand<asio::any_io_executor> strand_;
    std::vector<std::weak_ptr<Session>> sessions_{};
};

struct Session : std::enable_shared_from_this<Session>
{
    Session(tcp::socket socket, std::shared_ptr<Hub> hub, std::size_t max_line, std::size_t max_pending)
        : socket_(std::move(socket)),
          strand_(make_strand(socket_.get_executor())),
          hub_(std::move(hub)),
          max_line_(max_line),
          max_pending_(max_pending) {}

    auto start() -> void
    {
        auto const self = shared_from_this();
        hub_->join(self);
        co_spawn (
            strand_,
            [self] -> awaitable<void> {
                co_await self->run();
            },
            detached
        );
    }

    auto send_line(std::string msg) -> void
    {
        co_spawn (
            strand_,
            [self = shared_from_this(), msg = std::move(msg)]() mutable -> awaitable<void> {
                if (self->outbox_.size() >= self->max_pending_) {
                    std::println (
                        stderr,
                        "[warn] send drop (outbox full): nick='{}' size={} max={}",
                        self->nickname_,
                        self->outbox_.size(),
                        self->max_pending_
                    );
                    co_return;
                }
                self->outbox_.push_back(std::move(msg));
                if (not self->writing_) {
                    self->writing_ = true;
                    co_await self->writer();
                }
            },
            detached
        );
    }

private:
    auto writer() -> awaitable<void>
    {
        while (not outbox_.empty()) {
            send_buffer_.clear();
            send_buffer_.append(outbox_.front());
            send_buffer_.push_back('\n');

            auto [ec, _] = co_await async_write(socket_, buffer(send_buffer_), use_awaitable_noexcept);
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    std::println(stderr, "[info] write aborted for nick='{}'", nickname_);
                } else {
                    std::println(stderr, "[warn] write failed for nick='{}': {}", nickname_, ec.message());
                }
                break;
            }

            outbox_.pop_front();
        }

        writing_ = false;
        co_return;
    }

    auto run() -> awaitable<void>
    {
        auto buffer_str = std::string{};
        while (true) {
            auto [ec, _] = co_await async_read_until(
                socket_,
                dynamic_string_buffer(buffer_str, max_line_),
                '\n',
                use_awaitable_noexcept
            );
            if (ec) {
                if (ec == asio::error::eof) {
                    std::println(stderr, "[info] peer closed, nick='{}'", nickname_);
                } else if (ec == asio::error::operation_aborted) {
                    std::println(stderr, "[info] read aborted, nick='{}'", nickname_);
                } else {
                    std::println(stderr, "[warn] read error, nick='{}': {}", nickname_, ec.message());
                }
                break;
            }

            auto line = std::string_view{ buffer_str };
            if (auto pos = line.find('\n'); pos != std::string_view::npos) {
                line = line.substr(0, pos + 1);
            }
            // 先解析，再从缓冲区抹去，以免 erase() 使得 line 的 string_view 失效。
            auto cmd = protocol::parse_line(line);
            buffer_str.erase(0, line.size());
            if (cmd.kind == protocol::CommandKind::login) {
                auto nick = std::move(cmd.arg);
                if (nick.empty()) {
                    send_line(protocol::make_error("empty nickname"));
                    continue;
                }
                nickname_ = std::move(nick);
                send_line(protocol::make_hello_ack(nickname_));
            } else if (cmd.kind == protocol::CommandKind::say) {
                if (nickname_.empty()) {
                    send_line(protocol::make_error("login first"));
                    continue;
                }
                auto msg = protocol::make_msg_broadcast(nickname_, cmd.arg);
                if (auto h = hub_) {
                    h->broadcast(std::move(msg));
                }
            } else {
                send_line(protocol::make_error("unknown command"));
            }
        }
        co_return;
    }

    tcp::socket socket_;
    strand<asio::any_io_executor> strand_;
    std::shared_ptr<Hub> hub_{};
    std::string nickname_{};

    std::deque<std::string> outbox_{};
    bool writing_{ false };
    std::string send_buffer_{};

    std::size_t max_line_{};
    std::size_t max_pending_{};
};

auto inline session_send_line(std::shared_ptr<Session> const& s, std::string const& msg) -> void
{
    if (s) {
        s->send_line(msg);
    }
}

// 数字解析：使用 from_chars（十进制），失败返回空
template <std::unsigned_integral Unsigned>
auto inline parse_unsigned(std::string_view sv) noexcept -> std::optional<Unsigned>
{
    auto value = Unsigned{};
    auto const* first = sv.data();
    auto const* last = sv.data() + sv.size();
    auto const [ptr, ec] = std::from_chars(first, last, value, 10);
    if (ec != std::errc{} or ptr != last) {
        return std::nullopt;
    }
    return value;
}

auto get_env_or(std::string_view key, std::string_view def) -> std::string
{
    if (auto const* v = std::getenv(std::string(key).data())) {
        return std::string{ v };
    }
    return std::string{ def };
}

export auto read_server_config_from_env() -> ServerConfig
{
    auto cfg = ServerConfig{};
    cfg.addr = get_env_or("CHAT_ADDR", cfg.addr);
    if (auto p = get_env_or("CHAT_PORT", ""sv); not p.empty()) {
        if (auto v = parse_unsigned<std::uint16_t>(p)) {
            cfg.port = *v;
        } else {
            std::println(stderr, "[warn] invalid CHAT_PORT='{}', keep {}", p, cfg.port);
        }
    }
    if (auto t = get_env_or("CHAT_THREADS", ""sv); not t.empty()) {
        if (auto v = parse_unsigned<std::size_t>(t)) {
            cfg.threads = *v == 0 ? std::thread::hardware_concurrency() : *v;
        } else {
            std::println(stderr, "[warn] invalid CHAT_THREADS='{}', keep {}", t, cfg.threads);
        }
    }
    if (auto q = get_env_or("CHAT_MAX_PENDING_SEND", ""sv); not q.empty()) {
        if (auto v = parse_unsigned<std::size_t>(q)) {
            cfg.max_pending_send = *v;
        } else {
            std::println(stderr, "[warn] invalid CHAT_MAX_PENDING_SEND='{}', keep {}", q, cfg.max_pending_send);
        }
    }
    if (auto m = get_env_or("CHAT_MAX_LINE", ""sv); not m.empty()) {
        if (auto v = parse_unsigned<std::size_t>(m)) {
            cfg.max_line = *v;
        } else {
            std::println(stderr, "[warn] invalid CHAT_MAX_LINE='{}', keep {}", m, cfg.max_line);
        }
    }
    return cfg;
}

export struct Server
{
    explicit Server(ServerConfig cfg) : cfg_(std::move(cfg)), io_(cfg_.threads) {}

    auto run() -> void
    {
        auto const ex = io_.executor();
        auto const hub = std::make_shared<Hub>(ex);

        // 非抛异常地址解析与监听端点构建
        auto ec = boost::system::error_code{};
        auto const addr = asio::ip::make_address(cfg_.addr, ec);
        if (ec) {
            std::println(stderr, "[error] invalid address '{}': {}", cfg_.addr, ec.message());
            return;
        }
        auto const ep = tcp::endpoint(addr, cfg_.port);
        auto acceptor = tcp::acceptor(ex);
        acceptor.open(ep.protocol(), ec);
        if (ec) {
            std::println(stderr, "[error] acceptor.open failed: {}", ec.message());
            return;
        }
        acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
        if (ec) {
            std::println(stderr, "[error] acceptor.set_option(reuse_address) failed: {}", ec.message());
            return;
        }
        acceptor.bind(ep, ec);
        if (ec) {
            std::println(stderr, "[error] acceptor.bind failed: {}", ec.message());
            return;
        }
        acceptor.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            std::println(stderr, "[error] acceptor.listen failed: {}", ec.message());
            return;
        }

        auto accept_loop = [hub, &acceptor, this]() -> awaitable<void> {
            while (true) {
                auto [ec, socket] = co_await acceptor.async_accept(use_awaitable_noexcept);
                if (ec) {
                    if (ec == asio::error::operation_aborted) {
                        std::println(stderr, "[info] accept aborted");
                    } else {
                        std::println(stderr, "[warn] accept failed: {}", ec.message());
                    }
                    continue;
                }
                auto session = std::make_shared<Session>(std::move(socket), hub, cfg_.max_line, cfg_.max_pending_send);
                session->start();
            }
            co_return;
        };

        co_spawn(ex, accept_loop(), detached);

        std::println("Server listening on {}:{} , threads={}", cfg_.addr, cfg_.port, cfg_.threads);
        io_.run();
    }

    auto stop() -> void
    {
        io_.stop();
    }

private:
    ServerConfig cfg_{};
    IoRunner io_;
};
