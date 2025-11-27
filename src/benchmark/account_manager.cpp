#include "account_manager.h"
#include "data_store.h"

#include <iostream>
#include <format>

namespace benchmark
{
    AccountManager::AccountManager(asio::io_context& io, Config const& config)
        : io_{ io }
        , config_{ config }
    {
        // 初始化账号列表
        accounts_.reserve(config_.account_count);
        for(std::size_t i = 0; i < config_.account_count; ++i) {
            AccountInfo info;
            info.account = config_.make_account_name(i);
            info.group_index = config_.get_group_index(i);
            accounts_.push_back(std::move(info));
        }

        // 初始化观察者账号
        observer_.account = config_.make_observer_account();
        observer_.group_index = 0;  // 观察者会加入所有群

        // 初始化群聊列表
        groups_.reserve(config_.group_count);
        for(std::size_t i = 0; i < config_.group_count; ++i) {
            GroupInfo info;
            info.name = config_.make_group_name(i);
            groups_.push_back(std::move(info));
        }
    }

    auto AccountManager::setup() -> asio::awaitable<bool>
    {
        std::cout << "[AccountManager] Starting setup...\n";

        // 1. 注册账号
        if(!co_await register_accounts()) {
            std::cerr << "[AccountManager] Failed to register accounts\n";
            co_return false;
        }

        // 2. 登录账号获取 userId
        if(!co_await login_accounts()) {
            std::cerr << "[AccountManager] Failed to login accounts\n";
            co_return false;
        }

        // 3. 创建群聊
        if(!co_await create_groups()) {
            std::cerr << "[AccountManager] Failed to create groups\n";
            co_return false;
        }

        // 4. 保存数据到文件
        save_to_file();

        std::cout << "[AccountManager] Setup completed successfully!\n";
        co_return true;
    }

    auto AccountManager::register_accounts() -> asio::awaitable<bool>
    {
        std::cout << std::format(
            "[AccountManager] Registering {} accounts + 1 observer...\n",
            config_.account_count
        );

        std::size_t success_count = 0;
        std::size_t fail_count = 0;

        // 注册普通测试账号
        for(std::size_t i = 0; i < accounts_.size(); ++i) {
            auto client = std::make_shared<BenchmarkClient>(io_);

            if(!co_await client->async_connect(config_.server_host, config_.server_port)) {
                ++fail_count;
                continue;
            }

            auto const& account = accounts_[i];
            if(co_await client->async_register(account.account, config_.password)) {
                ++success_count;
            } else {
                // 可能已经注册过，也算成功
                ++success_count;
            }

            client->close();

            // 每 50 个账号输出一次进度
            if((i + 1) % 50 == 0) {
                std::cout << std::format(
                    "[AccountManager] Registered {}/{} accounts\n",
                    i + 1, accounts_.size()
                );
            }
        }

        // 注册观察者账号
        {
            auto client = std::make_shared<BenchmarkClient>(io_);
            if(co_await client->async_connect(config_.server_host, config_.server_port)) {
                co_await client->async_register(observer_.account, config_.password);
                client->close();
            }
        }

        std::cout << std::format(
            "[AccountManager] Registration complete: {} success, {} failed\n",
            success_count, fail_count
        );

        co_return true;
    }

    auto AccountManager::login_accounts() -> asio::awaitable<bool>
    {
        std::cout << std::format(
            "[AccountManager] Logging in {} accounts + 1 observer...\n",
            config_.account_count
        );

        std::size_t success_count = 0;

        // 登录普通测试账号
        for(std::size_t i = 0; i < accounts_.size(); ++i) {
            auto client = std::make_shared<BenchmarkClient>(io_);

            if(!co_await client->async_connect(config_.server_host, config_.server_port)) {
                std::cerr << std::format(
                    "[AccountManager] Failed to connect for account {}\n",
                    accounts_[i].account
                );
                continue;
            }

            auto user_id = co_await client->async_login(accounts_[i].account, config_.password);
            if(!user_id.empty()) {
                accounts_[i].user_id = user_id;
                ++success_count;
            } else {
                std::cerr << std::format(
                    "[AccountManager] Failed to login account {}\n",
                    accounts_[i].account
                );
            }

            client->close();

            // 每 50 个账号输出一次进度
            if((i + 1) % 50 == 0) {
                std::cout << std::format(
                    "[AccountManager] Logged in {}/{} accounts\n",
                    i + 1, accounts_.size()
                );
            }
        }

        // 登录观察者账号
        {
            auto client = std::make_shared<BenchmarkClient>(io_);
            if(co_await client->async_connect(config_.server_host, config_.server_port)) {
                auto user_id = co_await client->async_login(observer_.account, config_.password);
                if(!user_id.empty()) {
                    observer_.user_id = user_id;
                } else {
                    std::cerr << "[AccountManager] Failed to login observer account\n";
                }
                client->close();
            }
        }

        std::cout << std::format(
            "[AccountManager] Login complete: {} success\n",
            success_count
        );

        co_return success_count > 0;
    }

    auto AccountManager::create_groups() -> asio::awaitable<bool>
    {
        std::cout << std::format(
            "[AccountManager] Creating {} groups...\n",
            config_.group_count
        );

        // 为每个群分配成员
        for(std::size_t i = 0; i < accounts_.size(); ++i) {
            auto group_idx = accounts_[i].group_index;
            if(group_idx < groups_.size() && !accounts_[i].user_id.empty()) {
                groups_[group_idx].member_ids.push_back(accounts_[i].user_id);
            }
        }

        // 观察者加入所有群
        if(!observer_.user_id.empty()) {
            for(auto& group : groups_) {
                group.member_ids.push_back(observer_.user_id);
            }
        }

        // 使用第一个用户来创建所有群聊
        std::string creator_account;
        std::string creator_user_id;

        // 找到第一个有效的用户作为创建者
        for(auto const& account : accounts_) {
            if(!account.user_id.empty()) {
                creator_account = account.account;
                creator_user_id = account.user_id;
                break;
            }
        }

        if(creator_user_id.empty()) {
            std::cerr << "[AccountManager] No valid user to create groups\n";
            co_return false;
        }

        auto client = std::make_shared<BenchmarkClient>(io_);
        if(!co_await client->async_connect(config_.server_host, config_.server_port)) {
            std::cerr << "[AccountManager] Failed to connect for group creation\n";
            co_return false;
        }

        auto login_id = co_await client->async_login(creator_account, config_.password);
        if(login_id.empty()) {
            std::cerr << "[AccountManager] Failed to login for group creation\n";
            co_return false;
        }

        std::size_t success_count = 0;
        for(std::size_t i = 0; i < groups_.size(); ++i) {
            auto& group = groups_[i];

            // 创建群聊时排除创建者（服务器会自动加入创建者）
            std::vector<std::string> other_members;
            for(auto const& member_id : group.member_ids) {
                if(member_id != creator_user_id) {
                    other_members.push_back(member_id);
                }
            }

            auto conversation_id = co_await client->async_create_group(group.name, other_members);
            if(!conversation_id.empty()) {
                group.conversation_id = conversation_id;
                ++success_count;
                std::cout << std::format(
                    "[AccountManager] Created group '{}' with id {} ({} members)\n",
                    group.name, conversation_id, group.member_ids.size()
                );
            } else {
                std::cerr << std::format(
                    "[AccountManager] Failed to create group '{}'\n",
                    group.name
                );
            }
        }

        client->close();

        std::cout << std::format(
            "[AccountManager] Group creation complete: {}/{} success\n",
            success_count, groups_.size()
        );

        co_return success_count > 0;
    }

    auto AccountManager::get_conversation_id(std::size_t account_index) const -> std::string
    {
        if(account_index >= accounts_.size()) {
            return "";
        }

        auto group_index = accounts_[account_index].group_index;
        if(group_index >= groups_.size()) {
            return "";
        }

        return groups_[group_index].conversation_id;
    }

    auto AccountManager::load_from_file() -> bool
    {
        std::cout << "[AccountManager] Loading data from file...\n";
        if(DataStore::load(config_, accounts_, groups_, observer_)) {
            std::cout << std::format(
                "[AccountManager] Loaded {} accounts, {} groups from file\n",
                accounts_.size(), groups_.size()
            );
            return true;
        }
        std::cerr << "[AccountManager] Failed to load data from file\n";
        return false;
    }

    auto AccountManager::save_to_file() -> bool
    {
        std::cout << "[AccountManager] Saving data to file...\n";
        if(DataStore::save(config_, accounts_, groups_, observer_)) {
            std::cout << std::format(
                "[AccountManager] Saved to {}\n",
                DataStore::get_data_file_path(config_)
            );
            return true;
        }
        std::cerr << "[AccountManager] Failed to save data to file\n";
        return false;
    }

} // namespace benchmark
