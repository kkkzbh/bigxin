

export module client.cli.core;

import std;

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace client::cli
{
    export struct Args
    {
        std::string host{ "127.0.0.1"s };
        std::uint16_t port{ 7777 };
        std::string nick{};
        bool auto_login{ false };
    };

    export auto parse_args(int argc, char** argv) -> Args
    {
        auto a = Args{};
        auto host_set = false;
        auto port_set = false;
        auto nick_set = false;

        for (auto i : std::views::iota(1, argc)) {
            auto s = std::string_view{ argv[i] };
            if (s == "--host"sv and i + 1 < argc) {
                a.host = argv[++i];
                host_set = true;
            } else if (s == "--port"sv and i + 1 < argc) {
                auto sv = std::string_view{ argv[++i] };
                auto val = std::uint32_t{};
                auto const* first = sv.data();
                auto const* last = sv.data() + sv.size();
                auto const [ptr, ec] = std::from_chars(first, last, val, 10);
                if (ec == std::errc{} and ptr == last and val <= 65535) {
                    a.port = static_cast<std::uint16_t>(val);
                    port_set = true;
                }
            } else if (s == "--nick"sv and i + 1 < argc) {
                a.nick = argv[++i];
                a.auto_login = true;
                nick_set = true;
            }
        }

        if (not host_set) {
            if (auto const* v = std::getenv("CHAT_ADDR")) {
                a.host = std::string{ v };
            }
        }
        if (not port_set) {
            if (auto const* v = std::getenv("CHAT_PORT")) {
                auto sv = std::string_view{ v };
                auto val = std::uint32_t{};
                auto const* first = sv.data();
                auto const* last = sv.data() + sv.size();
                auto const [ptr, ec] = std::from_chars(first, last, val, 10);
                if (ec == std::errc{} and ptr == last and val <= 65535) {
                    a.port = static_cast<std::uint16_t>(val);
                }
            }
        }
        if (not nick_set) {
            if (auto const* v = std::getenv("CHAT_NICK")) {
                a.nick = std::string{ v };
                if (not a.nick.empty()) {
                    a.auto_login = true;
                }
            }
        }
        return a;
    }
}

namespace client::ui
{
    export struct UiMessage
    {
        std::string nick{};
        std::string content{};
        std::string ts{};
        bool is_sent{ false };
        bool is_system{ false };
    };

    export struct ChatState
    {
        std::string input{};
        std::string current_nick{};
        std::string status{};
        std::vector<UiMessage> messages{};
        std::mutex messages_mutex{};
        std::string host{};
        std::uint16_t port{ 0 };
    };

    export auto now_hms() -> std::string
    {
        auto const tp = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        return std::format("{:%H:%M:%S}", tp);
    }

    export auto push_sys(ChatState& st, std::string line) -> void
    {
        auto lock = std::scoped_lock{ st.messages_mutex };
        st.messages.push_back(UiMessage{
            .nick = std::string{},
            .content = std::move(line),
            .ts = now_hms(),
            .is_sent = false,
            .is_system = true,
        });
    }
}
