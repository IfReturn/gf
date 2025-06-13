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
    std::string session_id;  // 会话ID，用于关联多轮对话
    int turn_number;         // 在当前会话中的轮次编号
    
    HistoryEntry() : turn_number(0) {}
    HistoryEntry(const std::string& user_msg, const std::string& assistant_resp, 
                const std::string& sys_prompt = "", const std::string& model_name = "deepseek-chat",
                const std::string& sess_id = "", int turn_num = 0);
    
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
    std::string current_session_id;  // 当前会话ID
    int current_turn_number;         // 当前会话的轮次编号
    
    // 获取当前时间戳
    std::string get_current_timestamp() const;
    
    // 生成新的会话ID
    std::string generate_session_id() const;

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
     * @brief 开始新的会话
     * @return 新会话的ID
     */
    std::string start_new_session();
    
    /**
     * @brief 获取当前会话ID
     * @return 当前会话ID
     */
    std::string get_current_session_id() const;
    
    /**
     * @brief 设置当前会话ID（用于恢复会话）
     * @param session_id 会话ID
     */
    void set_current_session_id(const std::string& session_id);
    
    /**
     * @brief 添加历史记录条目（多轮对话版本）
     * @param user_message 用户消息
     * @param assistant_response 助手回复
     * @param system_prompt 系统提示（可选）
     * @param model 使用的模型（可选）
     */
    void add_entry_multi_turn(const std::string& user_message, const std::string& assistant_response,
                             const std::string& system_prompt = "", const std::string& model = "deepseek-chat");
    
    /**
     * @brief 获取指定会话的所有对话记录
     * @param session_id 会话ID
     * @return 该会话的历史记录
     */
    std::vector<HistoryEntry> get_session_history(const std::string& session_id) const;
    
    /**
     * @brief 获取所有会话ID列表
     * @return 会话ID向量
     */
    std::vector<std::string> get_all_session_ids() const;
    
    /**
     * @brief 显示会话列表
     */
    void display_sessions() const;
    
    /**
     * @brief 显示指定会话的对话记录
     * @param session_id 会话ID
     * @param show_details 是否显示详细信息
     */
    void display_session_history(const std::string& session_id, bool show_details = false) const;
    
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
