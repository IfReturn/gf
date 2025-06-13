#include "config.hpp"
#include <cstdlib>
#include <sys/stat.h>

Config::Config(const std::string& config_path) {
    if (config_path.empty()) {
        // 使用默认路径 ~/.config/gf/config.json
        const char* home = getenv("HOME");
        if (home) {
            config_file_path = std::string(home) + "/.config/gf/config.json";
        } else {
            config_file_path = "./config.json"; // 备用路径
        }
    } else {
        config_file_path = config_path;
    }
    
    load_defaults();
    ensure_config_directory();
}

void Config::load_defaults() {
    // 设置默认配置
    config_data["default_system_prompt"] = "You are a helpful assistant.";
    config_data["stream_enabled"] = true;
    config_data["max_history_entries"] = 1000;
    config_data["default_model"] = "deepseek-chat";
    config_data["auto_save_history"] = true;
    config_data["temperature"] = 0.7;
}

void Config::ensure_config_directory() {
    std::filesystem::path config_path(config_file_path);
    std::filesystem::path config_dir = config_path.parent_path();
    
    if (!std::filesystem::exists(config_dir)) {
        std::filesystem::create_directories(config_dir);
    }
}

bool Config::load_config() {
    std::ifstream config_file(config_file_path);
    if (!config_file.is_open()) {
        // 如果配置文件不存在，使用默认配置并保存
        std::cout << "Configuration file not found, creating with default settings: " 
                  << config_file_path << std::endl;
        return save_config();
    }
    
    Json::CharReaderBuilder reader;
    std::string errors;
    
    if (!Json::parseFromStream(reader, config_file, &config_data, &errors)) {
        std::cerr << "Error parsing configuration file: " << errors << std::endl;
        load_defaults(); // 重新加载默认配置
        return false;
    }
    
    config_file.close();
    return true;
}

bool Config::save_config() {
    std::ofstream config_file(config_file_path);
    if (!config_file.is_open()) {
        std::cerr << "Error: Cannot open configuration file for writing: " 
                  << config_file_path << std::endl;
        return false;
    }
    
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "  "; // 美化JSON格式
    std::unique_ptr<Json::StreamWriter> json_writer(writer.newStreamWriter());
    json_writer->write(config_data, &config_file);
    
    config_file.close();
    return true;
}

std::string Config::get_config_path() const {
    return config_file_path;
}

std::string Config::get_history_path() const {
    std::filesystem::path config_path(config_file_path);
    std::filesystem::path history_path = config_path.parent_path() / "history.json";
    return history_path.string();
}

std::string Config::get_default_system_prompt() const {
    return get<std::string>("default_system_prompt", "You are a helpful assistant.");
}

bool Config::get_stream_enabled() const {
    return get<bool>("stream_enabled", true);
}

int Config::get_max_history_entries() const {
    return get<int>("max_history_entries", 1000);
}

std::string Config::get_default_model() const {
    return get<std::string>("default_model", "deepseek-chat");
}
