module;


export module client.cli.command;

import std;
import client.cli.core;
import client.core;

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace client::cli::command
{
    export struct CommandResult
    {
        bool exit{ false };
        std::optional<std::string> system_msg{};
    };

    export auto handle_command(client::ui::ChatState& st, ChatClient& client, std::string_view line) -> CommandResult
    {
        if (line.starts_with('/')) {
            if (line.starts_with("/quit"sv)) {
                return CommandResult{ .exit = true, .system_msg = std::nullopt };
            }
            if (line.starts_with("/help"sv)) {
                return CommandResult{ .exit = false, .system_msg = "Commands: /nick <name>, /quit, /help"s };
            }
            if (line.starts_with("/nick "sv)) {
                auto nick = std::string(line.substr(6));
                client.hello(std::move(nick));
                return CommandResult{};
            }
            if (line.starts_with("/ping"sv)) {
                client.post("[ping]"s);
                return CommandResult{};
            }
            return CommandResult{ .exit = false, .system_msg = "[warn] unknown command"s };
        }
        return CommandResult{};
    }
}
