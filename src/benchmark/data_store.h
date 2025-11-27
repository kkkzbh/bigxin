#pragma once

#include "benchmark_config.h"
#include "account_manager.h"

#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
#include <filesystem>

namespace benchmark
{
    using json = nlohmann::json;

    /// \brief 压测数据持久化工具
    /// 用于保存和加载 setup 阶段的结果
    class DataStore
    {
    public:
        /// \brief 根据配置生成数据文件路径
        static auto get_data_file_path(Config const& config) -> std::string
        {
            return config.account_prefix + "benchmark_data.json";
        }

        /// \brief 保存账号和群聊数据到文件
        static auto save(
            Config const& config,
            std::vector<AccountInfo> const& accounts,
            std::vector<GroupInfo> const& groups,
            AccountInfo const& observer
        ) -> bool
        {
            try {
                json data;
                data["prefix"] = config.account_prefix;
                data["account_count"] = config.account_count;
                data["group_count"] = config.group_count;

                // 保存账号信息
                json accounts_json = json::array();
                for(auto const& acc : accounts) {
                    json acc_json;
                    acc_json["account"] = acc.account;
                    acc_json["user_id"] = acc.user_id;
                    acc_json["group_index"] = acc.group_index;
                    accounts_json.push_back(acc_json);
                }
                data["accounts"] = accounts_json;

                // 保存群聊信息
                json groups_json = json::array();
                for(auto const& grp : groups) {
                    json grp_json;
                    grp_json["name"] = grp.name;
                    grp_json["conversation_id"] = grp.conversation_id;
                    grp_json["member_ids"] = grp.member_ids;
                    groups_json.push_back(grp_json);
                }
                data["groups"] = groups_json;

                // 保存观察者信息
                json observer_json;
                observer_json["account"] = observer.account;
                observer_json["user_id"] = observer.user_id;
                observer_json["group_index"] = observer.group_index;
                data["observer"] = observer_json;

                // 写入文件
                auto file_path = get_data_file_path(config);
                std::ofstream file{ file_path };
                if(!file.is_open()) {
                    return false;
                }
                file << data.dump(2);
                return true;
            }
            catch(...) {
                return false;
            }
        }

        /// \brief 从文件加载账号和群聊数据
        static auto load(
            Config const& config,
            std::vector<AccountInfo>& accounts,
            std::vector<GroupInfo>& groups,
            AccountInfo& observer
        ) -> bool
        {
            try {
                auto file_path = get_data_file_path(config);
                if(!std::filesystem::exists(file_path)) {
                    return false;
                }

                std::ifstream file{ file_path };
                if(!file.is_open()) {
                    return false;
                }

                auto data = json::parse(file);

                // 验证配置匹配
                if(data["prefix"].get<std::string>() != config.account_prefix) {
                    return false;
                }

                // 加载账号信息
                accounts.clear();
                for(auto const& acc_json : data["accounts"]) {
                    AccountInfo acc;
                    acc.account = acc_json["account"].get<std::string>();
                    acc.user_id = acc_json["user_id"].get<std::string>();
                    acc.group_index = acc_json["group_index"].get<std::size_t>();
                    accounts.push_back(acc);
                }

                // 加载群聊信息
                groups.clear();
                for(auto const& grp_json : data["groups"]) {
                    GroupInfo grp;
                    grp.name = grp_json["name"].get<std::string>();
                    grp.conversation_id = grp_json["conversation_id"].get<std::string>();
                    grp.member_ids = grp_json["member_ids"].get<std::vector<std::string>>();
                    groups.push_back(grp);
                }

                // 加载观察者信息
                auto const& obs_json = data["observer"];
                observer.account = obs_json["account"].get<std::string>();
                observer.user_id = obs_json["user_id"].get<std::string>();
                observer.group_index = obs_json["group_index"].get<std::size_t>();

                return true;
            }
            catch(...) {
                return false;
            }
        }

        /// \brief 检查数据文件是否存在
        static auto exists(Config const& config) -> bool
        {
            return std::filesystem::exists(get_data_file_path(config));
        }
    };

} // namespace benchmark
