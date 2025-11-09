module;

#include <boost/asio.hpp>

export module client.core;

import std;
import io.core;
import protocol;
import utility;

namespace asio = boost::asio;

using util::use_awaitable_noexcept;
using namespace std::string_view_literals;
using namespace std::string_literals;
using asio::awaitable;
using asio::co_spawn;
using asio::detached;
using asio::ip::tcp;
using asio::async_read_until;
using asio::async_write;
using asio::dynamic_string_buffer;
using asio::buffer;
using asio::make_strand;
using asio::strand;

export struct ChatEventHandler
{
    std::function<void(std::string const& user)> on_hello_ack{};
    std::function<void(std::string const& user, std::string const& text)> on_broadcast{};
    std::function<void(std::string const& message)> on_error{};
    std::function<void()> on_closed{};
    std::function<void()> on_connected{};
};

export struct ChatClient : std::enable_shared_from_this<ChatClient>
{

    ChatClient() : io_(1), socket_(io_.executor()), strand_(make_strand(io_.executor())) {}

    auto set_handler(ChatEventHandler h) -> void
    {
        handler_ = std::move(h);
    }

    auto connect(std::string host, std::uint16_t port) -> void
    {
        if (running_) {
            return;
        }
        running_ = true;
        runner_ = std::jthread([this] { io_.run(); });

        co_spawn (
            strand_,
            [self = shared_from_this(), host = std::move(host), port] -> awaitable<void> {
                auto ex = self->socket_.get_executor();
                auto resolver = tcp::resolver(ex);
                auto const service = std::to_string(port);
                auto [ec_r, results] = co_await resolver.async_resolve(host, service, use_awaitable_noexcept);
                if (ec_r) {
                    self->emit_error(std::format("resolve failed: {}", ec_r.message()));
                    co_return;
                }

                auto [ec_c, ep] = co_await asio::async_connect(self->socket_, results, use_awaitable_noexcept);
                if (ec_c) {
                    self->emit_error(std::format("connect failed: {}", ec_c.message()));
                    co_return;
                }
                self->connected_ = true;
                if (not self->outbox_.empty() and not self->writing_) {
                    self->writing_ = true;
                    co_spawn (
                        self->strand_,
                        [self] -> awaitable<void> {
                            co_await self->writer();
                        },
                        detached
                    );
                }
                if (self->handler_.on_connected) {
                    self->handler_.on_connected();
                }
                co_await self->read_loop();
                self->connected_ = false;
            },
            detached
        );
    }

    auto hello(std::string nickname) -> void
    {
        nickname_ = std::move(nickname);
        send_line(protocol::make_login(nickname_));
    }

    auto post(std::string text) -> void
    {
        send_line(protocol::make_say(std::move(text)));
    }

    auto close() -> void
    {
        asio::dispatch (
            strand_,
            [self = shared_from_this()] {
                boost::system::error_code ec;
                self->socket_.shutdown(tcp::socket::shutdown_both, ec);
                self->socket_.close(ec);
            }
        );

        running_ = false;
        if (runner_.joinable()) {
            io_.stop();
            // jthread joins on destruction, but we can request stop via io_
        }
    }

private:
    auto send_line(std::string msg) -> void
    {
        co_spawn (
            strand_,
            [self = shared_from_this(), msg = std::move(msg)] mutable -> awaitable<void> {
                if (self->outbox_.size() >= self->max_pending_) {
                    self->emit_warn("drop: outbox full"s);
                    co_return;
                }
                self->outbox_.push_back(std::move(msg));
                if (self->connected_ and not self->writing_) {
                    self->writing_ = true;
                    co_await self->writer();
                }
            },
            detached
        );
    }

    auto writer() -> awaitable<void>
    {
        while (not outbox_.empty()) {
            send_buffer_.clear();
            send_buffer_.append(outbox_.front());
            send_buffer_.push_back('\n');

            auto [ec, _] = co_await async_write(socket_, buffer(send_buffer_), use_awaitable_noexcept);
            if (ec) {
                emit_warn(std::format("write failed: {}", ec.message()));
                break;
            }

            outbox_.pop_front();
        }
        writing_ = false;
    }

    auto read_loop() -> awaitable<void>
    {
        auto buf = std::string{};
        while (true) {
            auto [ec, _] = co_await async_read_until(
                socket_,
                dynamic_string_buffer(buf, max_line_),
                '\n',
                use_awaitable_noexcept
            );
            if (ec) {
                if (ec == asio::error::eof or ec == asio::error::operation_aborted) {
                    emit_info("connection closed"s);
                } else {
                    emit_warn(std::format("read error: {}", ec.message()));
                }
                if (handler_.on_closed) {
                    handler_.on_closed();
                }
                break;
            }

            auto line = std::string_view{ buf };
            if (auto pos = line.find('\n'); pos != std::string_view::npos) {
                line = line.substr(0, pos);
            }
            // 先处理，再从缓冲区抹去，以免 erase() 使 line 的 string_view 失效。
            handle_server_line(line);
            // erase consumed including '\n'
            buf.erase(0, line.size() + 1);
        }
        
    }

    auto handle_server_line(std::string_view line) -> void
    {
        // Server emits: HELLO_ACK:<nick> | MSG:<nick>:<text> | ERROR:<msg>
        auto const take_prefix = [&](std::string_view p) -> std::optional<std::string_view> {
            if (line.rfind(p, 0) == 0) {
                return line.substr(p.size());
            }
            return std::nullopt;
        };

        if (auto rest = take_prefix("HELLO_ACK:"sv)) {
            auto nick = std::string{ protocol::trim_crlf(*rest) };
            if (handler_.on_hello_ack) {
                handler_.on_hello_ack(nick);
            }
            return;
        }
        if (auto rest = take_prefix("MSG:"sv)) {
            auto payload = *rest;
            auto sep = payload.find(':');
            if (sep == std::string_view::npos) {
                emit_warn("bad MSG format"s);
                return;
            }
            auto nick = std::string{ payload.substr(0, sep) };
            auto text = std::string{ protocol::trim_crlf(payload.substr(sep + 1)) };
            if (handler_.on_broadcast) {
                handler_.on_broadcast(nick, text);
            }
            return;
        }
        if (auto rest = take_prefix("ERROR:"sv)) {
            auto msg = std::string{ protocol::trim_crlf(*rest) };
            emit_error("server: "s + msg);
            return;
        }
        auto raw = std::string{ line };
        for (auto& ch : raw) {
            if (ch == '\r' or ch == '\n') {
                ch = ' ';
            }
        }
        emit_warn("unknown line from server: '"s + raw + "'"s);
    }

    auto emit_info(std::string const& m) -> void
    {
        if (handler_.on_broadcast) {
            handler_.on_broadcast("[info]"s, m);
        }
    }
    auto emit_warn(std::string const& m) -> void
    {
        if (handler_.on_error) {
            handler_.on_error("[warn] "s + m);
        }
    }
    auto emit_error(std::string const& m) -> void
    {
        if (handler_.on_error) {
            handler_.on_error("[error] "s + m);
        }
    }

    IoRunner io_;
    tcp::socket socket_;
    strand<asio::any_io_executor> strand_;
    std::jthread runner_{};
    bool running_{ false };
    bool connected_{ false };

    std::string nickname_{};
    std::deque<std::string> outbox_{};
    bool writing_{ false };
    std::string send_buffer_{};
    std::size_t const max_pending_{ std::size_t{ 256 } };
    std::size_t const max_line_{ std::size_t{ 4096 } };

    ChatEventHandler handler_{};
};
