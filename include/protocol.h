#pragma once

#include <string>
#include <string_view>
#include <stdexcept>

/// \brief 文本协议的最小解析 / 拼装工具。
namespace protocol
{
    struct Frame
    {
        /// \brief 命令名，例如 SEND_MSG / PING。
        std::string command;
        /// \brief 冒号后的 JSON 文本。
        std::string payload;
    };

    /// \brief 解析一行形如 "COMMAND:{...}\\n" 的文本。
    /// \param line 包含命令和负载的整行字符串视图。
    /// \return 拆分后的 Frame 结构。
    /// \throws std::runtime_error 当行为空或缺少冒号时抛出。
    auto inline parse_line(std::string_view line) -> Frame
    {
        // 去掉结尾的 '\r' '\n'
        while(!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.remove_suffix(1);
        }
        if(line.empty()) {
            throw std::runtime_error{ "empty line" };
        }

        auto const pos = line.find(':');
        if(pos == std::string_view::npos) {
            throw std::runtime_error{ "protocol error: missing ':'" };
        }

        Frame f{};
        f.command.assign(line.substr(0, pos));
        f.payload.assign(line.substr(pos + 1));
        return f;
    }

    /// \brief 组装一行 "COMMAND:{...}\\n"。
    /// \param command 命令名。
    /// \param payload JSON 负载字符串。
    /// \return 已经带有结尾换行符的完整文本行。
    auto inline make_line(std::string_view command, std::string_view payload) -> std::string
    {
        std::string out;
        out.reserve(command.size() + payload.size() + 2);
        out.append(command);
        out.push_back(':');
        out.append(payload);
        out.push_back('\n');
        return out;
    }
} // namespace protocol
