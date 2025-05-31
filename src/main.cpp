#include "arg_parser.hpp" 
#include <cstdlib>
#include <iostream>
#include <json/json.h>
#include <curl/curl.h>
#include <json/value.h>
#include "deepseek.hpp"
#include <readline/readline.h>
#include <unistd.h>
int main(int argc,char** argv){
    arg_parser parser(argc, argv);
    auto positional_args = parser.get_positional_args();
    if (parser.has_option("--help")|| parser.has_option("-h")) {
        std::cout << "Usage: program [options] [args]\n";
        std::cout << "Options:\n";
        std::cout << "  --help          Show this help message\n";
        std::cout << "  --version       Show version information\n";
        return 0;
    }
    if (parser.has_option("--version")||parser.has_option("-v")) {
        std::cout << "Version 1.0.0\n";
        return 0;
    }
    for (const auto& arg : positional_args) {
        std::cout << "Positional argument: " << arg << "\n";
    }
    rl_initialize(); // 初始化readline库
    // 从环境变量获取API密钥
    std::string api_key = getenv("DEEPSEEK_API_KEY");
    if (api_key.empty()) {
        std::cerr << "Error: DEEPSEEK_API_KEY environment variable not set!" << std::endl;
        return 1;
    }
    deepseek ds(api_key);
    while(true){
        std::string prompt = readline("Ask: ");
        if (prompt.empty()) {
            std::cout << "Exiting..." << std::endl;
            break; // 如果输入为空，退出循环
        }
        // 发送请求并获取响应
        // 输出结果
            std::string answer = ds.ask("deepseek-chat", prompt);
            std::cout << "\n[DeepSeek回答]\n" << answer << std::endl;
    }
    return 0;
}