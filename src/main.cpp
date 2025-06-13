#include "arg_parser.hpp" 
#include <cstdlib>
#include <iostream>
#include <json/json.h>
#include <curl/curl.h>
#include <json/value.h>
#include "deepseek.hpp"
#include "config.hpp"
#include "history.hpp"
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <algorithm>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

// 全局变量用于信号处理
std::atomic<bool> g_running(true);
std::atomic<bool> g_interrupt_stream(false);
HistoryManager* g_history_manager = nullptr;
Config* g_config = nullptr;

// 全局变量用于跟踪当前对话状态
std::string g_current_user_input;
std::string g_current_assistant_response;
std::string g_current_system_prompt;
std::string g_current_model;
std::atomic<bool> g_conversation_in_progress(false);

// 信号处理函数 - 优化被打断时的历史保存
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
        g_interrupt_stream = true;
        
        // 静默中断当前操作
        rl_done = 1;
        
        // 给程序一点时间自然退出
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // 保存未完成的对话
        if (g_conversation_in_progress.load() && g_history_manager && !g_current_user_input.empty()) {
            // 如果正在进行对话，保存用户输入和已有的回复（即使是部分回复）
            std::string response_to_save = g_current_assistant_response.empty() ? 
                "[对话被中断]" : g_current_assistant_response + " [已中断]";
            
            g_history_manager->add_entry_multi_turn(
                g_current_user_input, 
                response_to_save, 
                g_current_system_prompt, 
                g_current_model
            );
        }
        
        // 静默保存数据并退出
        if (g_history_manager) {
            g_history_manager->save_history();
        }
        if (g_config) {
            g_config->save_config();
        }
        
        // 安静退出，只显示简单的告别
        std::cout << "\n" << std::endl;
        std::exit(0);
    }
}

// 自定义readline信号处理
void setup_readline_signals() {
    // 让readline处理信号，但我们仍然可以捕获它们
    rl_catch_signals = 1;
    rl_catch_sigwinch = 1;
}

// 安全的readline函数，能够正确处理信号中断
char* safe_readline(const char* prompt) {
    char* line = nullptr;
    
    while (g_running.load()) {
        line = readline(prompt);
        
        // 如果readline返回NULL，可能是因为EOF或信号中断
        if (!line) {
            if (!g_running.load()) {
                // 被信号中断
                break;
            }
            // EOF情况，用户按了Ctrl+D
            break;
        }
        
        // 正常获得输入
        return line;
    }
    
    return nullptr;
}
int main(int argc,char** argv){
    // 设置readline信号处理
    setup_readline_signals();
    
    // 注册信号处理器
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // 终止信号
    
    arg_parser parser(argc, argv);
    auto positional_args = parser.get_positional_args();
    if (parser.has_option("--help")|| parser.has_option("-h")) {
        std::cout << "Usage: program [options] [args]\n";
        std::cout << "Options:\n";
        std::cout << "  -h|--help                   Show this help message\n";
        std::cout << "  -v|--version                Show version information\n";
        std::cout << "  -s|--stream [on|off]        Enable streaming mode or not,default to on\n";
        std::cout << "  -c|--config <path>          Specify configuration file path\n";
        std::cout << "  --history [show|clear|search|sessions] History management commands\n";
        std::cout << "  --history-count <num>       Number of history entries to show (default: 10)\n";
        std::cout << "  --session <session_id>      Continue specific session or 'new' for new session\n";
        std::cout << "  --load-context <session_id> Load conversation context from session\n";
        std::cout << "  --max-context <num>         Maximum context turns to load (default: 10)\n";
        std::cout << "  --no-history                Disable history saving for this session\n";
        return 0;
    }

    if (parser.has_option("--version")|| parser.has_option("-v")) {
        std::cout << "Version 1.0.0\n";
        return 0;
    }
    
    // 加载配置文件
    std::string config_path = parser.get_option_value("--config");
    if (config_path.empty() && parser.has_option("-c")) {
        config_path = parser.get_option_value("-c");
    }
    Config config(config_path);
    if (!config.load_config()) {
        std::cerr << "Warning: Failed to load configuration, using defaults." << std::endl;
    }
    
    // 设置全局配置指针用于信号处理
    g_config = &config;
    
    // 初始化历史记录管理器
    bool enable_history = !parser.has_option("--no-history");
    HistoryManager* history_manager = nullptr;
    if (enable_history) {
        history_manager = new HistoryManager(config.get_history_path(), config.get_max_history_entries());
        if (!history_manager->load_history()) {
            std::cerr << "Warning: Failed to load history, starting with empty history." << std::endl;
        }
        // 设置全局历史记录管理器指针用于信号处理
        g_history_manager = history_manager;
    }
    
    // 处理历史记录命令（不需要API密钥）
    if (parser.has_option("--history")) {
        if (!history_manager) {
            std::cerr << "Error: History is disabled (--no-history was used)." << std::endl;
            return 1;
        }
        
        std::string history_cmd = parser.get_option_value("--history");
        
        if (history_cmd == "show") {
            int count = 10; // 默认显示10条
            if (parser.has_option("--history-count")) {
                count = std::stoi(parser.get_option_value("--history-count"));
            }
            history_manager->display_history(count, true);
        } else if (history_cmd == "clear") {
            history_manager->clear_history();
            history_manager->save_history();
            std::cout << "History cleared successfully." << std::endl;
        } else if (history_cmd == "search") {
            std::cout << "Enter search keyword: ";
            std::string keyword;
            std::getline(std::cin, keyword);
            if (!keyword.empty()) {
                auto results = history_manager->search_history(keyword);
                if (results.empty()) {
                    std::cout << "No matching history entries found." << std::endl;
                } else {
                    std::cout << "\nFound " << results.size() << " matching entries:" << std::endl;
                    for (size_t i = 0; i < results.size(); ++i) {
                        const auto& entry = results[i];
                        std::cout << "\n[" << (i + 1) << "] " << entry.timestamp;
                        if (!entry.session_id.empty()) {
                            std::cout << " (Session: " << entry.session_id << ", Turn: " << entry.turn_number << ")";
                        }
                        std::cout << std::endl;
                        std::cout << "User: " << entry.user_message << std::endl;
                        std::cout << "Assistant: " << entry.assistant_response.substr(0, 200);
                        if (entry.assistant_response.length() > 200) std::cout << "...";
                        std::cout << std::endl;
                    }
                }
            }
        } else if (history_cmd == "sessions") {
            history_manager->display_sessions();
            
            // 提供交互式会话查看功能
            std::cout << "\nEnter session number to view details (or press Enter to exit): ";
            std::string input;
            std::getline(std::cin, input);
            
            if (!input.empty()) {
                try {
                    int session_num = std::stoi(input);
                    auto session_ids = history_manager->get_all_session_ids();
                    if (session_num > 0 && session_num <= static_cast<int>(session_ids.size())) {
                        const std::string& selected_session = session_ids[session_num - 1];
                        history_manager->display_session_history(selected_session, true);
                    } else {
                        std::cout << "Invalid session number." << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cout << "Invalid input. Please enter a number." << std::endl;
                }
            }
        } else {
            std::cerr << "Invalid history command. Use 'show', 'clear', or 'search'." << std::endl;
        }
        
        // 清理并退出（历史记录命令不进入聊天模式）
        delete history_manager;
        return 0;
    }
    
    // 处理流式输出设置
    bool is_stream = config.get_stream_enabled(); // 从配置文件获取默认值
    if(parser.has_option("--stream") || parser.has_option("-s")) {
        auto stream_value = parser.get_option_value("--stream");
        if (stream_value.empty()) {
            stream_value = parser.get_option_value("-s");
        }
        if (stream_value == "off" || stream_value == "false") {
            is_stream = false; // 禁用流式输出
        } else if (stream_value == "on" || stream_value == "true" || stream_value.empty()) {
            is_stream = true; // 启用流式输出
        } else {
            std::cerr << "Invalid value for --stream. Use 'on' or 'off'.\n";
            if (history_manager) delete history_manager;
            return 1;
        }
    }
    
    // 对于聊天功能，需要检查API密钥
    rl_initialize(); // 初始化readline库
    // 从环境变量获取API密钥
    std::string api_key = getenv("DEEPSEEK_API_KEY")?getenv("DEEPSEEK_API_KEY"):"";
    if (api_key.empty()) {
        std::cerr << "Error: DEEPSEEK_API_KEY environment variable not set!" << std::endl;
        if (history_manager) delete history_manager;
        return 1;
    }
    deepseek ds(api_key, is_stream, history_manager);
    
    // 处理会话相关参数
    std::string session_to_use;
    if (parser.has_option("--session")) {
        session_to_use = parser.get_option_value("--session");
        if (session_to_use == "new") {
            if (history_manager) {
                session_to_use = ds.start_new_session();
                std::cout << "Started new session: " << session_to_use << std::endl;
            }
        } else {
            // 验证会话是否存在
            if (history_manager) {
                auto session_ids = history_manager->get_all_session_ids();
                bool session_exists = std::find(session_ids.begin(), session_ids.end(), session_to_use) != session_ids.end();
                if (session_exists) {
                    ds.set_current_session(session_to_use);
                    std::cout << "Continuing session: " << session_to_use << std::endl;
                } else {
                    std::cout << "Session not found: " << session_to_use << std::endl;
                    std::cout << "Available sessions:" << std::endl;
                    history_manager->display_sessions();
                    if (history_manager) delete history_manager;
                    return 1;
                }
            }
        }
    }
    
    // 处理上下文加载
    if (parser.has_option("--load-context")) {
        std::string context_session = parser.get_option_value("--load-context");
        int max_context = 10; // 默认加载10轮对话
        if (parser.has_option("--max-context")) {
            max_context = std::stoi(parser.get_option_value("--max-context"));
        }
        
        if (history_manager) {
            auto session_ids = history_manager->get_all_session_ids();
            bool session_exists = std::find(session_ids.begin(), session_ids.end(), context_session) != session_ids.end();
            if (session_exists) {
                ds.load_session_context(context_session, max_context);
                std::cout << "Loaded context from session: " << context_session 
                          << " (max " << max_context << " turns)" << std::endl;
            } else {
                std::cout << "Context session not found: " << context_session << std::endl;
                if (history_manager) delete history_manager;
                return 1;
            }
        }
    }
    
    // 设置系统提示
    std::string default_prompt = config.get_default_system_prompt();
    std::string sysprompt = readline(("Waiting for system prompt, default: \"" + default_prompt + "\": ").c_str());
    if (!sysprompt.empty()) {
        ds.set_system_prompt(sysprompt);
    } else {
        ds.set_system_prompt(default_prompt);
    }
    
    std::cout << "\nChat started! Type your questions below. Press Enter on empty line to exit.\n";
    std::cout << "Configuration file: " << config.get_config_path() << std::endl;
    if (history_manager) {
        std::cout << "History file: " << config.get_history_path() << std::endl;
        std::cout << "Current history entries: " << history_manager->get_history_count() << std::endl;
        std::cout << "Current session: " << ds.get_current_session_id() << std::endl;
    }
    std::cout << "Stream mode: " << (is_stream ? "enabled" : "disabled") << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    while(g_running.load()){
        char* line = safe_readline("Ask: ");
        
        // 检查是否被信号中断或到达EOF
        if (!line) {
            // 静默退出，不显示任何信息
            break;
        }
        
        std::string prompt(line);
        free(line);  // 释放readline分配的内存
        
        if (prompt.empty()) {
            break; // 如果输入为空，静默退出循环
        }
        
        // 处理特殊命令
        if (prompt == "/help") {
            std::cout << "\nSpecial commands:\n";
            std::cout << "  /help         - Show this help\n";
            std::cout << "  /new          - Start new session\n";
            std::cout << "  /session      - Show current session info\n";
            std::cout << "  /sessions     - List all sessions\n";
            std::cout << "  /load <id>    - Load session context\n";
            std::cout << "  /clear        - Clear current conversation context\n";
            std::cout << "  /exit         - Exit the program\n";
            continue;
        } else if (prompt == "/new") {
            if (history_manager) {
                std::string new_session = ds.start_new_session();
                std::cout << "Started new session: " << new_session << std::endl;
            }
            continue;
        } else if (prompt == "/session") {
            if (history_manager) {
                std::cout << "Current session: " << ds.get_current_session_id() << std::endl;
                auto session_history = history_manager->get_session_history(ds.get_current_session_id());
                std::cout << "Session turns: " << session_history.size() << std::endl;
            }
            continue;
        } else if (prompt == "/sessions") {
            if (history_manager) {
                history_manager->display_sessions();
            }
            continue;
        } else if (prompt.substr(0, 6) == "/load ") {
            if (history_manager) {
                std::string session_id = prompt.substr(6);
                auto session_ids = history_manager->get_all_session_ids();
                if (std::find(session_ids.begin(), session_ids.end(), session_id) != session_ids.end()) {
                    ds.load_session_context(session_id, 10);
                    ds.set_current_session(session_id);
                    std::cout << "Loaded context from session: " << session_id << std::endl;
                } else {
                    std::cout << "Session not found: " << session_id << std::endl;
                }
            }
            continue;
        } else if (prompt == "/clear") {
            ds.clear_conversation_context();
            std::cout << "Conversation context cleared." << std::endl;
            continue;
        } else if (prompt == "/exit") {
            std::cout << "Exiting..." << std::endl;
            break;
        }
        
        // 检查是否在处理命令时被中断
        if (!g_running.load()) {
            break; // 静默退出
        }
        
        // 设置当前对话状态（用于信号处理时保存）
        g_current_user_input = prompt;
        g_current_assistant_response = "";
        g_current_system_prompt = ds.get_system_prompt();
        g_current_model = config.get_default_model();
        g_conversation_in_progress = true;
        
        std::cout << "\n[DeepSeek回答]\n"  << std::endl;
        // 发送请求并获取响应
        std::string response = ds.ask(config.get_default_model(), prompt);
        
        // 对话完成，清除状态
        g_conversation_in_progress = false;
        g_current_user_input = "";
        g_current_assistant_response = "";
        
        // 检查响应是否因为中断而异常
        if (!g_running.load() || response.empty()) {
            break; // 静默退出
        }
        
        std::cout << "\n";
    }
    
    // 静默保存数据
    if (history_manager) {
        history_manager->save_history();
        delete history_manager;
    }
    
    // 静默保存配置
    config.save_config();
    
    return 0;
}