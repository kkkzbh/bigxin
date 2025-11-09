 
 
// Client CLI entry (non-module TU)
#include <ftxui/component/screen_interactive.hpp>

import std;
import client.core;
import client.cli.core;
import client.cli.command;
import client.cli.ui;

using namespace std::string_literals;

auto main(int argc, char** argv) -> int
{
    auto const args = client::cli::parse_args(argc, argv);

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    auto st = client::ui::ChatState{
        .input = std::string{},
        .current_nick = args.nick,
        .status = "[connecting]"s,
        .messages = {},
        .messages_mutex = {},
        .host = args.host,
        .port = args.port,
    };

    auto client = std::make_shared<ChatClient>();
    client->set_handler({
        .on_hello_ack = [&screen, &st](std::string const& user) {
            screen.Post([&st, user] {
                client::ui::push_sys(st, std::format("[info] hello, {}", user));
                st.current_nick = user;
                st.status = "[ready]"s;
            });
            // 关键：从网络线程唤醒 UI 事件循环，确保立即重绘。
            screen.PostEvent(ftxui::Event::Custom);
        },
        .on_broadcast = [&screen, &st](std::string const& user, std::string const& text) {
            screen.Post([&st, user, text] {
                auto const is_me = (not st.current_nick.empty()) and (user == st.current_nick);
                auto lock = std::scoped_lock{ st.messages_mutex };
                st.messages.push_back(client::ui::UiMessage{
                    .nick = user,
                    .content = text,
                    .ts = client::ui::now_hms(),
                    .is_sent = is_me,
                    .is_system = false,
                });
            });
            screen.PostEvent(ftxui::Event::Custom);
        },
        .on_error = [&screen, &st](std::string const& msg) {
            screen.Post([&st, msg] {
                client::ui::push_sys(st, msg);
                st.status = "[error]"s;
            });
            screen.PostEvent(ftxui::Event::Custom);
        },
        .on_closed = [&screen, &st] {
            screen.Post([&st] {
                client::ui::push_sys(st, "[info] closed"s);
                st.status = "[closed]"s;
            });
            screen.PostEvent(ftxui::Event::Custom);
        },
        .on_connected = [&screen, &st] {
            screen.Post([&st] { st.status = "[connected]"s; });
            screen.PostEvent(ftxui::Event::Custom);
        },
    });

    client->connect(st.host, st.port);
    if (args.auto_login and not args.nick.empty()) {
        client->hello(args.nick);
    }

    auto chat = make_chat_component(st, *client, screen);
    screen.Loop(chat);

    client->close();
    return 0;
}
