#pragma once
#include <atomic>
#include <string>
#include <memory>
#include "history.hpp"
#include "config.hpp"

/**
 * @brief 单例类用于管理所有全局变量
 * 
 * 这个类使用单例模式来管理应用程序中的所有全局状态，
 * 包括中断标志、对话状态、配置和历史管理器等。
 */
class GlobalManager {
private:
    // 私有构造函数，防止直接实例化
    GlobalManager() = default;
    
    // 禁用拷贝构造和赋值操作
    GlobalManager(const GlobalManager&) = delete;
    GlobalManager& operator=(const GlobalManager&) = delete;

    // 全局运行状态
    std::atomic<bool> running_{true};
    std::atomic<bool> interrupt_stream_{false};
    std::atomic<bool> conversation_in_progress_{false};
    
    // 对话相关状态
    std::string current_user_input_;
    std::string current_assistant_response_;
    std::string current_system_prompt_;
    std::string current_model_;
    
    // 管理器指针
    HistoryManager* history_manager_ = nullptr;
    Config* config_ = nullptr;

public:
    /**
     * @brief 获取GlobalManager的单例实例
     * @return GlobalManager的单例引用
     */
    static GlobalManager& getInstance() {
        static GlobalManager instance;
        return instance;
    }

    // 运行状态管理
    bool isRunning() const { return running_.load(); }
    void setRunning(bool running) { running_.store(running); }
    
    bool isInterruptStream() const { return interrupt_stream_.load(); }
    void setInterruptStream(bool interrupt) { interrupt_stream_.store(interrupt); }
    
    bool isConversationInProgress() const { return conversation_in_progress_.load(); }
    void setConversationInProgress(bool in_progress) { conversation_in_progress_.store(in_progress); }

    // 对话状态管理
    const std::string& getCurrentUserInput() const { return current_user_input_; }
    void setCurrentUserInput(const std::string& input) { current_user_input_ = input; }
    
    const std::string& getCurrentAssistantResponse() const { return current_assistant_response_; }
    void setCurrentAssistantResponse(const std::string& response) { current_assistant_response_ = response; }
    
    const std::string& getCurrentSystemPrompt() const { return current_system_prompt_; }
    void setCurrentSystemPrompt(const std::string& prompt) { current_system_prompt_ = prompt; }
    
    const std::string& getCurrentModel() const { return current_model_; }
    void setCurrentModel(const std::string& model) { current_model_ = model; }

    // 管理器指针管理
    HistoryManager* getHistoryManager() const { return history_manager_; }
    void setHistoryManager(HistoryManager* manager) { history_manager_ = manager; }
    
    Config* getConfig() const { return config_; }
    void setConfig(Config* config) { config_ = config; }

    /**
     * @brief 重置所有状态到初始值
     */
    void reset() {
        running_.store(true);
        interrupt_stream_.store(false);
        conversation_in_progress_.store(false);
        current_user_input_.clear();
        current_assistant_response_.clear();
        current_system_prompt_.clear();
        current_model_.clear();
        // 注意：不重置管理器指针，因为它们可能仍在使用
    }

    /**
     * @brief 清理对话相关状态
     */
    void clearConversationState() {
        current_user_input_.clear();
        current_assistant_response_.clear();
        conversation_in_progress_.store(false);
    }

    /**
     * @brief 保存当前状态（用于信号处理）
     */
    void saveCurrentState() {
        if (conversation_in_progress_.load() && history_manager_ && !current_user_input_.empty()) {
            std::string response_to_save = current_assistant_response_.empty() ? 
                "[对话被中断]" : current_assistant_response_ + " [已中断]";
            
            history_manager_->add_entry_multi_turn(
                current_user_input_, 
                response_to_save, 
                current_system_prompt_, 
                current_model_
            );
        }
        
        if (history_manager_) {
            history_manager_->save_history();
        }
        
        if (config_) {
            config_->save_config();
        }
    }
};
