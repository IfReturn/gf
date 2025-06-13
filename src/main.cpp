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
#include <unistd.h>
int main(int argc,char** argv){
    arg_parser parser(argc, argv);
    auto positional_args = parser.get_positional_args();
    if (parser.has_option("--help")|| parser.has_option("-h")) {
        std::cout << "Usage: program [options] [args]\n";
        std::cout << "Options:\n";
        std::cout << "  -h|--help                   Show this help message\n";
        std::cout << "  -v|--version                Show version information\n";
        std::cout << "  -s|--stream [on|off]        Enable streaming mode or not,default to on\n";
        std::cout << "  -c|--config <path>          Specify configuration file path\n";
        std::cout << "  --history [show|clear|search] History management commands\n";
        std::cout << "  --history-count <num>       Number of history entries to show (default: 10)\n";
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
    
    // 初始化历史记录管理器
    bool enable_history = !parser.has_option("--no-history");
    HistoryManager* history_manager = nullptr;
    if (enable_history) {
        history_manager = new HistoryManager(config.get_history_path(), config.get_max_history_entries());
        if (!history_manager->load_history()) {
            std::cerr << "Warning: Failed to load history, starting with empty history." << std::endl;
        }
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
                        std::cout << "\n[" << (i + 1) << "] " << entry.timestamp << std::endl;
                        std::cout << "User: " << entry.user_message << std::endl;
                        std::cout << "Assistant: " << entry.assistant_response.substr(0, 200);
                        if (entry.assistant_response.length() > 200) std::cout << "...";
                        std::cout << std::endl;
                    }
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
    }
    std::cout << "Stream mode: " << (is_stream ? "enabled" : "disabled") << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    while(true){
        std::string prompt = readline("Ask: ");
        if (prompt.empty()) {
            std::cout << "Exiting..." << std::endl;
            break; // 如果输入为空，退出循环
        }
        
        std::cout << "\n[DeepSeek回答]\n"  << std::endl;
        // 发送请求并获取响应
        ds.ask(config.get_default_model(), prompt);
        std::cout << "\n";
    }
    
    // 清理并保存历史记录
    if (history_manager) {
        if (!history_manager->save_history()) {
            std::cerr << "Warning: Failed to save history." << std::endl;
        }
        delete history_manager;
    }
    
    // 保存配置
    if (!config.save_config()) {
        std::cerr << "Warning: Failed to save configuration." << std::endl;
    }
    return 0;
}