#include "history.hpp"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <filesystem>

HistoryEntry::HistoryEntry(const std::string& user_msg, const std::string& assistant_resp, 
                         const std::string& sys_prompt, const std::string& model_name,
                         const std::string& sess_id, int turn_num)
    : user_message(user_msg), assistant_response(assistant_resp), 
      system_prompt(sys_prompt), model(model_name), session_id(sess_id), turn_number(turn_num) {
    // 生成时间戳
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    timestamp = ss.str();
}

Json::Value HistoryEntry::to_json() const {
    Json::Value json;
    json["timestamp"] = timestamp;
    json["user_message"] = user_message;
    json["assistant_response"] = assistant_response;
    json["system_prompt"] = system_prompt;
    json["model"] = model;
    json["session_id"] = session_id;
    json["turn_number"] = turn_number;
    return json;
}

HistoryEntry HistoryEntry::from_json(const Json::Value& json) {
    HistoryEntry entry;
    entry.timestamp = json.get("timestamp", "").asString();
    entry.user_message = json.get("user_message", "").asString();
    entry.assistant_response = json.get("assistant_response", "").asString();
    entry.system_prompt = json.get("system_prompt", "").asString();
    entry.model = json.get("model", "deepseek-chat").asString();
    entry.session_id = json.get("session_id", "").asString();
    entry.turn_number = json.get("turn_number", 0).asInt();
    return entry;
}

HistoryManager::HistoryManager(const std::string& history_path, int max_entries)
    : history_file_path(history_path), max_entries(max_entries), current_turn_number(0) {
    // 确保历史记录目录存在
    std::filesystem::path history_file(history_path);
    std::filesystem::path history_dir = history_file.parent_path();
    
    if (!std::filesystem::exists(history_dir)) {
        std::filesystem::create_directories(history_dir);
    }
    
    // 开始新会话
    start_new_session();
}

std::string HistoryManager::get_current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

bool HistoryManager::load_history() {
    std::ifstream history_file(history_file_path);
    if (!history_file.is_open()) {
        // 如果历史文件不存在，创建空的历史记录
        std::cout << "History file not found, creating new history file: " 
                  << history_file_path << std::endl;
        return save_history();
    }
    
    Json::CharReaderBuilder reader;
    Json::Value root;
    std::string errors;
    
    if (!Json::parseFromStream(reader, history_file, &root, &errors)) {
        std::cerr << "Error parsing history file: " << errors << std::endl;
        history_file.close();
        return false;
    }
    
    history_file.close();
    
    // 清空现有历史记录
    history_entries.clear();
    
    // 加载历史记录
    if (root.isMember("history") && root["history"].isArray()) {
        for (const auto& entry_json : root["history"]) {
            history_entries.push_back(HistoryEntry::from_json(entry_json));
        }
    }
    
    return true;
}

bool HistoryManager::save_history() {
    // 如果历史记录超过最大限制，删除最旧的记录
    while (static_cast<int>(history_entries.size()) > max_entries) {
        history_entries.erase(history_entries.begin());
    }
    
    Json::Value root;
    Json::Value history_array(Json::arrayValue);
    
    for (const auto& entry : history_entries) {
        history_array.append(entry.to_json());
    }
    
    root["history"] = history_array;
    root["total_entries"] = static_cast<int>(history_entries.size());
    root["last_updated"] = get_current_timestamp();
    
    std::ofstream history_file(history_file_path);
    if (!history_file.is_open()) {
        std::cerr << "Error: Cannot open history file for writing: " 
                  << history_file_path << std::endl;
        return false;
    }
    
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> json_writer(writer.newStreamWriter());
    json_writer->write(root, &history_file);
    
    history_file.close();
    return true;
}

void HistoryManager::add_entry(const std::string& user_message, const std::string& assistant_response,
                              const std::string& system_prompt, const std::string& model) {
    HistoryEntry entry(user_message, assistant_response, system_prompt, model);
    history_entries.push_back(entry);
    
    // 如果超过最大限制，删除最旧的记录
    if (static_cast<int>(history_entries.size()) > max_entries) {
        history_entries.erase(history_entries.begin());
    }
}

const std::vector<HistoryEntry>& HistoryManager::get_history() const {
    return history_entries;
}

std::vector<HistoryEntry> HistoryManager::get_recent_history(int count) const {
    if (count <= 0 || count >= static_cast<int>(history_entries.size())) {
        return history_entries;
    }
    
    return std::vector<HistoryEntry>(
        history_entries.end() - count, history_entries.end());
}

void HistoryManager::clear_history() {
    history_entries.clear();
}

size_t HistoryManager::get_history_count() const {
    return history_entries.size();
}

std::vector<HistoryEntry> HistoryManager::search_history(const std::string& keyword, 
                                                       bool search_user_messages,
                                                       bool search_assistant_responses) const {
    std::vector<HistoryEntry> results;
    
    for (const auto& entry : history_entries) {
        bool match = false;
        
        if (search_user_messages) {
            if (entry.user_message.find(keyword) != std::string::npos) {
                match = true;
            }
        }
        
        if (search_assistant_responses && !match) {
            if (entry.assistant_response.find(keyword) != std::string::npos) {
                match = true;
            }
        }
        
        if (match) {
            results.push_back(entry);
        }
    }
    
    return results;
}

void HistoryManager::display_history(int count, bool show_details) const {
    std::vector<HistoryEntry> entries_to_show;
    
    if (count == -1 || count >= static_cast<int>(history_entries.size())) {
        entries_to_show = history_entries;
    } else {
        entries_to_show = get_recent_history(count);
    }
    
    if (entries_to_show.empty()) {
        std::cout << "No history entries found." << std::endl;
        return;
    }
    
    std::cout << "\n=== Chat History (" << entries_to_show.size() << " entries) ===" << std::endl;
    
    for (size_t i = 0; i < entries_to_show.size(); ++i) {
        const auto& entry = entries_to_show[i];
        
        std::cout << "\n[" << (i + 1) << "] " << entry.timestamp;
        if (show_details && !entry.model.empty()) {
            std::cout << " (Model: " << entry.model << ")";
        }
        std::cout << std::endl;
        
        // 显示用户消息（截断长消息）
        std::string user_msg = entry.user_message;
        if (!show_details && user_msg.length() > 100) {
            user_msg = user_msg.substr(0, 100) + "...";
        }
        std::cout << "User: " << user_msg << std::endl;
        
        // 显示助手回复（截断长回复）
        std::string assistant_msg = entry.assistant_response;
        if (!show_details && assistant_msg.length() > 200) {
            assistant_msg = assistant_msg.substr(0, 200) + "...";
        }
        std::cout << "Assistant: " << assistant_msg << std::endl;
        
        if (show_details && !entry.system_prompt.empty()) {
            std::cout << "System Prompt: " << entry.system_prompt << std::endl;
        }
    }
    
    std::cout << "\n=== End of History ===" << std::endl;
}

std::string HistoryManager::generate_session_id() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << "session_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string HistoryManager::start_new_session() {
    current_session_id = generate_session_id();
    current_turn_number = 0;
    return current_session_id;
}

std::string HistoryManager::get_current_session_id() const {
    return current_session_id;
}

void HistoryManager::set_current_session_id(const std::string& session_id) {
    current_session_id = session_id;
    // 找到该会话的最大轮次编号
    current_turn_number = 0;
    for (const auto& entry : history_entries) {
        if (entry.session_id == session_id && entry.turn_number > current_turn_number) {
            current_turn_number = entry.turn_number;
        }
    }
}

void HistoryManager::add_entry_multi_turn(const std::string& user_message, const std::string& assistant_response,
                                         const std::string& system_prompt, const std::string& model) {
    current_turn_number++;
    HistoryEntry entry(user_message, assistant_response, system_prompt, model, current_session_id, current_turn_number);
    history_entries.push_back(entry);
    
    // 如果超过最大限制，删除最旧的记录
    if (static_cast<int>(history_entries.size()) > max_entries) {
        history_entries.erase(history_entries.begin());
    }
}

std::vector<HistoryEntry> HistoryManager::get_session_history(const std::string& session_id) const {
    std::vector<HistoryEntry> session_entries;
    for (const auto& entry : history_entries) {
        if (entry.session_id == session_id) {
            session_entries.push_back(entry);
        }
    }
    
    // 按轮次编号排序
    std::sort(session_entries.begin(), session_entries.end(),
              [](const HistoryEntry& a, const HistoryEntry& b) {
                  return a.turn_number < b.turn_number;
              });
    
    return session_entries;
}

std::vector<std::string> HistoryManager::get_all_session_ids() const {
    std::vector<std::string> session_ids;
    for (const auto& entry : history_entries) {
        if (std::find(session_ids.begin(), session_ids.end(), entry.session_id) == session_ids.end()) {
            session_ids.push_back(entry.session_id);
        }
    }
    
    // 按时间排序（会话ID包含时间戳）
    std::sort(session_ids.begin(), session_ids.end());
    return session_ids;
}

void HistoryManager::display_sessions() const {
    auto session_ids = get_all_session_ids();
    
    if (session_ids.empty()) {
        std::cout << "No chat sessions found." << std::endl;
        return;
    }
    
    std::cout << "\n=== Chat Sessions ===" << std::endl;
    for (size_t i = 0; i < session_ids.size(); ++i) {
        const auto& session_id = session_ids[i];
        auto session_entries = get_session_history(session_id);
        
        std::cout << "\n[" << (i + 1) << "] Session: " << session_id << std::endl;
        std::cout << "    Turns: " << session_entries.size() << std::endl;
        
        if (!session_entries.empty()) {
            std::cout << "    Started: " << session_entries.front().timestamp << std::endl;
            std::cout << "    Last: " << session_entries.back().timestamp << std::endl;
            
            // 显示第一轮对话的简要内容
            const auto& first_entry = session_entries.front();
            std::string preview = first_entry.user_message;
            if (preview.length() > 50) {
                preview = preview.substr(0, 50) + "...";
            }
            std::cout << "    Preview: " << preview << std::endl;
        }
    }
    std::cout << "\n=== End of Sessions ===" << std::endl;
}

void HistoryManager::display_session_history(const std::string& session_id, bool show_details) const {
    auto session_entries = get_session_history(session_id);
    
    if (session_entries.empty()) {
        std::cout << "No entries found for session: " << session_id << std::endl;
        return;
    }
    
    std::cout << "\n=== Session History: " << session_id << " ===" << std::endl;
    std::cout << "Total turns: " << session_entries.size() << std::endl;
    
    for (const auto& entry : session_entries) {
        std::cout << "\n--- Turn " << entry.turn_number << " (" << entry.timestamp << ") ---" << std::endl;
        
        // 显示用户消息
        std::string user_msg = entry.user_message;
        if (!show_details && user_msg.length() > 100) {
            user_msg = user_msg.substr(0, 100) + "...";
        }
        std::cout << "User: " << user_msg << std::endl;
        
        // 显示助手回复
        std::string assistant_msg = entry.assistant_response;
        if (!show_details && assistant_msg.length() > 200) {
            assistant_msg = assistant_msg.substr(0, 200) + "...";
        }
        std::cout << "Assistant: " << assistant_msg << std::endl;
        
        if (show_details) {
            std::cout << "Model: " << entry.model << std::endl;
            if (!entry.system_prompt.empty()) {
                std::cout << "System Prompt: " << entry.system_prompt << std::endl;
            }
        }
    }
    
    std::cout << "\n=== End of Session ===" << std::endl;
}
