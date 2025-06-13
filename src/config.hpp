#pragma once
#include <string>
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <filesystem>

class Config {
private:
    std::string config_file_path;
    Json::Value config_data;
    
    // 默认配置
    void load_defaults();
    // 确保配置目录存在
    void ensure_config_directory();
    
public:
    /**
     * @brief 构造函数，指定配置文件路径
     * @param config_path 配置文件路径，默认为 ~/.config/gf/config.json
     */
    Config(const std::string& config_path = "");
    
    /**
     * @brief 从文件加载配置
     * @return 是否成功加载
     */
    bool load_config();
    
    /**
     * @brief 保存配置到文件
     * @return 是否成功保存
     */
    bool save_config();
    
    /**
     * @brief 获取配置项的值
     * @param key 配置项的键
     * @param default_value 默认值
     * @return 配置项的值
     */
    template<typename T>
    T get(const std::string& key, const T& default_value) const;
    
    /**
     * @brief 设置配置项的值
     * @param key 配置项的键
     * @param value 配置项的值
     */
    template<typename T>
    void set(const std::string& key, const T& value);
    
    /**
     * @brief 获取配置文件路径
     * @return 配置文件路径
     */
    std::string get_config_path() const;
    
    /**
     * @brief 获取历史记录文件路径
     * @return 历史记录文件路径
     */
    std::string get_history_path() const;
    
    /**
     * @brief 获取默认系统提示
     * @return 默认系统提示
     */
    std::string get_default_system_prompt() const;
    
    /**
     * @brief 获取是否启用流式输出
     * @return 是否启用流式输出
     */
    bool get_stream_enabled() const;
    
    /**
     * @brief 获取历史记录最大保存条数
     * @return 历史记录最大保存条数
     */
    int get_max_history_entries() const;
    
    /**
     * @brief 获取默认模型名称
     * @return 默认模型名称
     */
    std::string get_default_model() const;
};

// 模板函数的实现
template<typename T>
T Config::get(const std::string& key, const T& default_value) const {
    if (config_data.isMember(key)) {
        if constexpr (std::is_same_v<T, std::string>) {
            return config_data[key].asString();
        } else if constexpr (std::is_same_v<T, bool>) {
            return config_data[key].asBool();
        } else if constexpr (std::is_same_v<T, int>) {
            return config_data[key].asInt();
        } else if constexpr (std::is_same_v<T, double>) {
            return config_data[key].asDouble();
        }
    }
    return default_value;
}

template<typename T>
void Config::set(const std::string& key, const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        config_data[key] = value;
    } else if constexpr (std::is_same_v<T, bool>) {
        config_data[key] = value;
    } else if constexpr (std::is_same_v<T, int>) {
        config_data[key] = value;
    } else if constexpr (std::is_same_v<T, double>) {
        config_data[key] = value;
    }
}
