module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

export module client.cli.ui;

import std;
import client.core;
import client.cli.command;
import client.cli.core; // ChatClient

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{
    struct Theme
    {
        ftxui::Color bg{ ftxui::Color::RGB(0x29, 0x28, 0x38) };
        ftxui::Color fg{ ftxui::Color::RGB(0xE5, 0xE7, 0xEB) };
        ftxui::Color sep{ ftxui::Color::RGB(0x37, 0x32, 0x4C) };

        ftxui::Color sent_bg{ ftxui::Color::RGB(0x3F, 0x4B, 0x8A) };
        ftxui::Color sent_border{ ftxui::Color::RGB(0x4F, 0x46, 0xE5) };
        ftxui::Color sent_text{ ftxui::Color::RGB(0xF9, 0xFA, 0xFB) };
        ftxui::Color sent_meta{ ftxui::Color::RGB(0xC7, 0xD2, 0xFE) };

        ftxui::Color recv_bg{ ftxui::Color::RGB(0x34, 0x32, 0x4A) };
        ftxui::Color recv_border{ ftxui::Color::RGB(0x4B, 0x55, 0x63) };
        ftxui::Color recv_text{ ftxui::Color::RGB(0xE5, 0xE7, 0xEB) };
        ftxui::Color recv_meta{ ftxui::Color::RGB(0x9C, 0xA3, 0xAF) };

        ftxui::Color sys_time{ ftxui::Color::RGB(0x6B, 0x72, 0x80) };
        ftxui::Color sys_text{ ftxui::Color::RGB(0xCB, 0xD5, 0xF5) };
        ftxui::Color info{ ftxui::Color::RGB(0x38, 0xBD, 0xF8) };
        ftxui::Color warn{ ftxui::Color::RGB(0xFA, 0xCC, 0x15) };
        ftxui::Color error{ ftxui::Color::RGB(0xF9, 0x73, 0x16) };

        ftxui::Color header_bg{ ftxui::Color::RGB(0x1E, 0x1E, 0x2E) };
        ftxui::Color header_text{ ftxui::Color::RGB(0xE5, 0xE7, 0xEB) };
        ftxui::Color header_nick{ ftxui::Color::RGB(0xC7, 0xD2, 0xFE) };
        ftxui::Color status_ready{ ftxui::Color::RGB(0x22, 0xC5, 0x5E) };
        ftxui::Color status_conn{ ftxui::Color::RGB(0xFA, 0xCC, 0x15) };
        ftxui::Color status_err{ ftxui::Color::RGB(0xF9, 0x73, 0x16) };
        ftxui::Color status_closed{ ftxui::Color::RGB(0x9C, 0xA3, 0xAF) };

        ftxui::Color input_bar_bg{ ftxui::Color::RGB(0x20, 0x20, 0x30) };
        ftxui::Color input_field_bg{ ftxui::Color::RGB(0x2A, 0x2A, 0x3C) };
        ftxui::Color input_text{ ftxui::Color::RGB(0xE5, 0xE7, 0xEB) };
        ftxui::Color input_prefix{ ftxui::Color::RGB(0x38, 0xBD, 0xF8) };
        ftxui::Color input_hint{ ftxui::Color::RGB(0x9C, 0xA3, 0xAF) };
        ftxui::Color input_border{ ftxui::Color::RGB(0x4B, 0x55, 0x63) };
    };
    auto const UI_THEME = Theme{};

} // namespace

export auto make_chat_component(client::ui::ChatState& state,
                                ChatClient& client,
                                ftxui::ScreenInteractive& screen) -> ftxui::Component
{
    // 修复要点：必须先完成组件的 Renderer/CatchEvent 等装饰，然后再放入容器。
    // 之前的实现是在放入 Container 之后再对局部变量执行 `|=`，导致容器内持有旧的
    // Component 句柄（值语义 handle），渲染的输入框与接收事件/聚焦的不是同一个实例，
    // 从而输入无法接收键盘事件。

    // Input component（先配置 option，再构建 Input 并装饰）
    auto input_opt = ftxui::InputOption{};
    input_opt.multiline = false;
    input_opt.insert = true;
    input_opt.on_enter = [&state, &client, &screen] {
        if (state.input.empty()) {
            return;
        }
        auto line = std::string{};
        line.swap(state.input);
        if (line.starts_with('/')) {
            auto r = client::cli::command::handle_command(state, client, std::move(line));
            if (r.system_msg) {
                client::ui::push_sys(state, *r.system_msg);
            }
            if (r.exit) {
                screen.Exit();
            }
            return;
        }
        client.post(std::move(line));
    };
    input_opt.transform = [](ftxui::InputState s) -> ftxui::Element {
        auto e = s.element;
        e |= ftxui::color(s.is_placeholder ? UI_THEME.input_hint : UI_THEME.input_text);
        return e | ftxui::bgcolor(UI_THEME.input_field_bg);
    };

    auto input = ftxui::Input(&state.input, "Type message or /nick <name>"s, input_opt);

    // 将局部 UI 状态集中到共享对象中，保证生命周期覆盖整个组件树
    struct UiState {
        float scroll_ratio{ 1.0f };   // 0=顶部, 1=底部
        bool stick_to_bottom{ true }; // 新消息时自动跟随到底部
        std::size_t prev_msg_count{ 0 };
        ftxui::Box messages_box{};    // 命中检测
        struct DragState {
            bool active{ false };
            int origin_y{ 0 };
            float origin_ratio{ 1.0f };
        } drag{};
    };
    auto ui = std::make_shared<UiState>();

    // Messages list renderer：仅构建内容，不包 frame，让外层统一控制滚动
    auto list_renderer = ftxui::Renderer(
        [ui, &state] {
            auto lock = std::scoped_lock{ state.messages_mutex };
            auto elements = ftxui::Elements{};
            elements.reserve(state.messages.size() * 2);

            for (auto const& m : state.messages) {
                if (m.is_system) {
                    auto content_sv = std::string_view{ m.content };
                    auto tag_sv = std::string_view{};
                    auto body_sv = content_sv;
                    if (not content_sv.empty() and content_sv.front() == '[') {
                        if (auto const pos = content_sv.find(']'); pos != std::string_view::npos) {
                            tag_sv = content_sv.substr(0, pos + 1);
                            body_sv = content_sv.substr(pos + 1);
                            if (not body_sv.empty() and body_sv.front() == ' ') {
                                body_sv.remove_prefix(1);
                            }
                        }
                    }
                    auto const tag_col = [&]() -> ftxui::Color {
                        if (tag_sv == "[info]"sv) return UI_THEME.info;
                        if (tag_sv == "[warn]"sv) return UI_THEME.warn;
                        if (tag_sv == "[error]"sv) return UI_THEME.error;
                        return UI_THEME.sys_text;
                    }();
                    elements.push_back(
                        ftxui::hbox({
                            ftxui::text(std::format("[{}] ", m.ts)) | ftxui::color(UI_THEME.sys_time),
                            ftxui::text(std::string(tag_sv)) | ftxui::color(tag_col),
                            ftxui::text(tag_sv.empty() ? std::string{ m.content }
                                                       : std::string(body_sv))
                                | ftxui::color(UI_THEME.sys_text),
                        })
                    );
                    elements.push_back(ftxui::separator() | ftxui::color(UI_THEME.sep));
                    continue;
                }

                auto bubble = ftxui::vbox({
                    ftxui::text(m.content)
                        | ftxui::color(m.is_sent ? UI_THEME.sent_text : UI_THEME.recv_text)
                        | ftxui::bold,
                    ftxui::text(std::format("{} {}", m.nick, m.ts))
                        | ftxui::color(m.is_sent ? UI_THEME.sent_meta : UI_THEME.recv_meta),
                });
                auto decorated = bubble
                                 | ftxui::color(m.is_sent ? UI_THEME.sent_border : UI_THEME.recv_border)
                                 | ftxui::borderRounded
                                 | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 56)
                                 | ftxui::bgcolor(m.is_sent ? UI_THEME.sent_bg : UI_THEME.recv_bg);
                elements.push_back(m.is_sent ? ftxui::hbox({ ftxui::filler(), std::move(decorated) })
                                             : ftxui::hbox({ std::move(decorated), ftxui::filler() }));
                elements.push_back(ftxui::separator() | ftxui::color(UI_THEME.sep));
            }

            // 新消息到达时，如仍处于“跟随底部”，则将滚动位置拉到底部
            if (ui->prev_msg_count != state.messages.size() and ui->stick_to_bottom) {
                ui->scroll_ratio = 1.0f;
                ui->prev_msg_count = state.messages.size();
            } else {
                ui->prev_msg_count = state.messages.size();
            }

            return ftxui::vbox(std::move(elements));
        }
    );

    // 将消息区包装为可独立处理事件的组件，且在此处就完成 CatchEvent 装饰
    auto messages_comp_core = ftxui::Renderer(
        list_renderer,
        [ui, list = list_renderer] {
            auto messages_panel = (
                    list->Render()
                    | ftxui::vscroll_indicator
                    | ftxui::focusPositionRelative(0.0f, ui->scroll_ratio)
                    | ftxui::frame
                    | ftxui::flex
                )
                | ftxui::border
                | ftxui::reflect(ui->messages_box);
            return messages_panel;
        }
    );

    // 仅在消息组件上捕获鼠标滚动/拖拽，避免影响输入框键盘事件；
    // 注意：此 CatchEvent 必须在加入容器之前完成，保证容器内与聚焦用的是同一实例。
    auto messages_comp = ftxui::CatchEvent(
        messages_comp_core,
        // NOLINTNEXTLINE(*-identifier-length): FTXUI 回调参数名按库约定
        [ui, input](ftxui::Event e) {
            // 鼠标滚轮滚动（仅在消息区域内时生效）
            if (e.is_mouse()) {
                auto& m = e.mouse();
                auto const in_box = (m.x >= ui->messages_box.x_min and m.x <= ui->messages_box.x_max
                                     and m.y >= ui->messages_box.y_min and m.y <= ui->messages_box.y_max);
                if (in_box) {
                    if (m.button == ftxui::Mouse::WheelUp) {
                        ui->scroll_ratio = std::max(0.0f, ui->scroll_ratio - 0.08f);
                        ui->stick_to_bottom = (ui->scroll_ratio >= 0.999f);
                        return true; // 消费事件，避免容器切换焦点
                    }
                    if (m.button == ftxui::Mouse::WheelDown) {
                        ui->scroll_ratio = std::min(1.0f, ui->scroll_ratio + 0.08f);
                        ui->stick_to_bottom = (ui->scroll_ratio >= 0.999f);
                        return true;
                    }
                }
            }

            // 拖拽滚动（按住左键在消息区域内上下拖动）
            if (e.is_mouse()) {
                auto& m = e.mouse();
                auto const in_box = (m.x >= ui->messages_box.x_min and m.x <= ui->messages_box.x_max
                                     and m.y >= ui->messages_box.y_min and m.y <= ui->messages_box.y_max);

                if (m.button == ftxui::Mouse::Left and m.motion == ftxui::Mouse::Pressed and in_box) {
                    ui->drag.active = true;
                    ui->drag.origin_y = m.y;
                    ui->drag.origin_ratio = ui->scroll_ratio;
                    return true; // 消费按下，阻止容器切换焦点
                }
                if (ui->drag.active and m.motion == ftxui::Mouse::Moved) {
                    auto const view_h = std::max(1, ui->messages_box.y_max - ui->messages_box.y_min + 1);
                    auto const dy = m.y - ui->drag.origin_y;
                    auto const delta = -static_cast<float>(dy) / static_cast<float>(view_h);
                    ui->scroll_ratio = std::clamp(ui->drag.origin_ratio + delta, 0.0f, 1.0f);
                    ui->stick_to_bottom = (ui->scroll_ratio >= 0.999f);
                    return true; // 拖动中持续消费事件
                }
                if (ui->drag.active and (m.motion == ftxui::Mouse::Released or not in_box)) {
                    ui->drag.active = false;
                    // 鼠标释放后，将焦点交还给输入框，保证键盘输入不丢失。
                    input->TakeFocus();
                    return true;
                }
            }

            return false;
        }
    );

    // 发送消息时，强制跟随底部（在加入容器前装饰输入框）
    input = ftxui::CatchEvent(
        input,
        [ui, &state](ftxui::Event const& e) {
            if (e == ftxui::Event::Return and not state.input.empty()) {
                ui->scroll_ratio = 1.0f;
                ui->stick_to_bottom = true;
            }
            return false;
        }
    );

    // Focus tree — 把“已装饰完成的”组件塞进容器，确保句柄一致
    auto container = ftxui::Container::Vertical({ messages_comp, input });
    // 确保启动时焦点在输入框
    container->SetActiveChild(input);
    input->TakeFocus();

    // Root renderer — capture child components by value 以避免悬垂引用
    auto root = ftxui::Renderer(
        container,
        [&, ui, msg = messages_comp, in = input] {
            auto const online = (state.status == "[ready]"sv or state.status == "[connected]"sv);
            auto const err_or_closed = (state.status == "[error]"sv or state.status == "[closed]"sv);
            auto const status_color = online ? UI_THEME.status_ready
                                             : (err_or_closed ? (state.status == "[closed]"sv ? UI_THEME.status_closed
                                                                                              : UI_THEME.status_err)
                                                               : UI_THEME.status_conn);

            auto header =
                ftxui::hbox({
                    ftxui::text(" 💬 ") | ftxui::bold | ftxui::color(UI_THEME.header_text),
                    ftxui::text(std::format("聊天窗口  {}:{}  as ", state.host, state.port))
                        | ftxui::bold | ftxui::color(UI_THEME.header_text),
                    ftxui::text(state.current_nick)
                        | ftxui::bold | ftxui::color(UI_THEME.header_nick) | ftxui::flex,
                    ftxui::text(state.status) | ftxui::bold | ftxui::color(status_color),
                    ftxui::text("  在线 ") | ftxui::color(UI_THEME.header_text),
                    ftxui::text("●") | ftxui::color(status_color) | ftxui::bold,
                })
                | ftxui::bgcolor(UI_THEME.header_bg)
                | ftxui::border;

            auto messages_panel = msg->Render();
            auto const term_width = std::max(1, screen.dimx());
            auto const content_width = std::max(10, term_width - 8);
            auto const logical_len = static_cast<int>(state.input.size());
            auto const wrapped_lines = std::max(1, (logical_len + content_width - 1) / content_width);
            auto const explicit_lines = 1 + static_cast<int>(std::ranges::count(state.input, '\n'));
            auto const input_lines = std::clamp(std::max(wrapped_lines, explicit_lines), 1, 8);

            auto input_panel = (
                    ftxui::hbox({
                        ftxui::text(" > ") | ftxui::color(UI_THEME.input_prefix) | ftxui::bold,
                        in->Render() | ftxui::flex,
                        ftxui::text(" [Enter 发送] ") | ftxui::color(UI_THEME.input_hint),
                    })
                    | ftxui::bgcolor(UI_THEME.input_bar_bg)
                    | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, input_lines)
                )
                | ftxui::color(UI_THEME.input_border)
                | ftxui::borderRounded;

            return ftxui::vbox({
                       std::move(header),
                       std::move(messages_panel),
                       ftxui::separator() | ftxui::color(UI_THEME.sep),
                       std::move(input_panel),
                   })
                   | ftxui::color(UI_THEME.fg)
                   | ftxui::bgcolor(UI_THEME.bg)
                   | ftxui::borderDouble;
        }
    );

    return root;
}
