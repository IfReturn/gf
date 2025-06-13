#pragma once
#include <string>
#include <vector>
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <chrono>

struct HistoryEntry {
    std::string timestamp;
    std::string user_message;
    std::string assistant_response;
    std::string system_prompt;
    std::string model;
    
    HistoryEntry() = default;
    HistoryEntry(const std::string& user_msg, const std::string& assistant_resp, 
                const std::string& sys_prompt = "", const std::string& model_name = "deepseek-chat");
    
    // 转换为JSON
    Json::Value to_json() const;
    // 从JSON创建
    static HistoryEntry from_json(const Json::Value& json);
};

class HistoryManager {
private:
    std::string history_file_path;
    std::vector<HistoryEntry> history_entries;
    int max_entries;
    
    // 获取当前时间戳
    std::string get_current_timestamp() const;
    
public:
    /**
     * @brief 构造函数
     * @param history_path 历史记录文件路径
     * @param max_entries 最大历史记录条数
     */
    HistoryManager(const std::string& history_path, int max_entries = 1000);
    
    /**
     * @brief 从文件加载历史记录
     * @return 是否成功加载
     */
    bool load_history();
    
    /**
     * @brief 保存历史记录到文件
     * @return 是否成功保存
     */
    bool save_history();
    
    /**
     * @brief 添加历史记录条目
     * @param user_message 用户消息
     * @param assistant_response 助手回复
     * @param system_prompt 系统提示（可选）
     * @param model 使用的模型（可选）
     */
    void add_entry(const std::string& user_message, const std::string& assistant_response,
                   const std::string& system_prompt = "", const std::string& model = "deepseek-chat");
    
    /**
     * @brief 获取所有历史记录
     * @return 历史记录向量
     */
    const std::vector<HistoryEntry>& get_history() const;
    
    /**
     * @brief 获取最近的N条历史记录
     * @param count 要获取的记录数
     * @return 历史记录向量
     */
    std::vector<HistoryEntry> get_recent_history(int count) const;
    
    /**
     * @brief 清空历史记录
     */
    void clear_history();
    
    /**
     * @brief 获取历史记录总数
     * @return 历史记录条数
     */
    size_t get_history_count() const;
    
    /**
     * @brief 搜索历史记录
     * @param keyword 搜索关键词
     * @param search_user_messages 是否搜索用户消息
     * @param search_assistant_responses 是否搜索助手回复
     * @return 匹配的历史记录
     */
    std::vector<HistoryEntry> search_history(const std::string& keyword, 
                                           bool search_user_messages = true,
                                           bool search_assistant_responses = true) const;
    
    /**
     * @brief 显示历史记录
     * @param count 显示的记录数，-1表示显示所有
     * @param show_details 是否显示详细信息
     */
    void display_history(int count = -1, bool show_details = false) const;
};
