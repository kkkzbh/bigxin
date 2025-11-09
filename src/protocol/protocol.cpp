module;

export module protocol;

import std;

namespace protocol
{
    // 简单行文本协议（M1 临时）：
    //  - 登录:  LOGIN:<nickname>\n
    //  - 发言:  SAY:<text>\n
    // 服务器广播: MSG:<nickname>:<text>\n
    // 注意：M1 阶段用于最小验证，后续将切换为 Protobuf + 帧协议

    export enum class CommandKind
    {
        invalid,
        login,
        say,
    };

    export struct Command
    {
        CommandKind kind{ CommandKind::invalid };
        std::string arg{}; // 对于 login 是 nickname；对于 say 是 text
    };

    export auto inline trim_crlf(std::string_view s) noexcept -> std::string_view
    {
        while (not s.empty() and (s.back() == '\n' or s.back() == '\r')) {
            s.remove_suffix(1);
        }
        return s;
    }

    export auto parse_line(std::string_view line) -> Command
    {
        line = trim_crlf(line);
        if (line.empty()) {
            return {};
        }

        auto const pos = line.find(':');
        if (pos == std::string_view::npos) {
            return {};
        }

        auto cmd = line.substr(0, pos);
        auto arg = std::string{ line.substr(pos + 1) };

        // 大写比较（容错：允许小写）
        auto const to_upper = [](std::string_view x) {
            std::string r;
            r.reserve(x.size());
            for (char c : x) {
                r.push_back(std::toupper(static_cast<unsigned char>(c)));
            }
            return r;
        };

        auto up = to_upper(cmd);
        if (up == "LOGIN") {
            return { CommandKind::login, std::move(arg) };
        }
        if (up == "SAY") {
            return { CommandKind::say, std::move(arg) };
        }
        return {};
    }

    export auto make_msg_broadcast(std::string_view nickname, std::string_view text) -> std::string
    {
        std::string out;
        out.reserve(5 + nickname.size() + 1 + text.size());
        out.append("MSG:");
        out.append(nickname);
        out.push_back(':');
        out.append(text);
        return out;
    }

    export auto make_error(std::string_view message) -> std::string
    {
        std::string out;
        out.reserve(6 + message.size());
        out.append("ERROR:");
        out.append(message);
        return out;
    }

    export auto make_hello_ack(std::string_view nickname) -> std::string
    {
        std::string out;
        out.reserve(10 + nickname.size());
        out.append("HELLO_ACK:");
        out.append(nickname);
        return out;
    }

    export auto make_login(std::string_view nickname) -> std::string
    {
        std::string out;
        out.reserve(6 + nickname.size());
        out.append("LOGIN:");
        out.append(nickname);
        return out;
    }

    export auto make_say(std::string_view text) -> std::string
    {
        std::string out;
        out.reserve(4 + text.size());
        out.append("SAY:");
        out.append(text);
        return out;
    }
} // namespace protocol
