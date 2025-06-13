#include "deepseek.hpp"
#include <iostream>
#include <sstream>

// 辅助函数：尝试从一行data: ... JSON中提取content
static std::string extract_stream_content(const std::string &line) {
  if (line.find("data: ") != 0)
    return "";
  std::string json_part = line.substr(6); // 跳过"data: "
  Json::CharReaderBuilder reader;
  Json::Value root;
  std::string errors;
  std::istringstream json_stream(json_part);
  if (!Json::parseFromStream(reader, json_stream, &root, &errors))
    return "";
  if (root.isMember("choices") && root["choices"].isArray() &&
      !root["choices"].empty()) {
    const auto &choice = root["choices"][0];
    if (choice.isMember("delta") && choice["delta"].isMember("content")) {
      return choice["delta"]["content"].asString();
    }
  }
  return "";
}

size_t deepseek::WriteCallback(void *contents, size_t size, size_t nmemb,
                               std::string *data) {
  // 检查是否需要中断流式传输
  if (g_interrupt_stream.load()) {
    return 0;
  }
  
  size_t total_size = size * nmemb;
  data->append((char *)contents, total_size);
  static std::string full_content; // 用于多轮对话收集
  size_t pos = 0;
  while (true) {
    // 在处理每一行时也检查中断标志
    if (g_interrupt_stream.load()) {
      // 保存当前已有的部分响应到全局变量
      if (g_conversation_in_progress.load()) {
        g_current_assistant_response = full_content;
      }
      return total_size; // 返回已处理的大小
    }
    
    size_t next = data->find("\n", pos);
    if (next == std::string::npos)
      break;
    std::string line = data->substr(pos, next - pos);
    std::string content = extract_stream_content(line);
    if (!content.empty()) {
      std::cout << content << std::flush;
      full_content += content;
      
      // 实时更新全局响应状态（用于信号处理）
      if (g_conversation_in_progress.load()) {
        g_current_assistant_response = full_content;
      }
    }
    pos = next + 1;
  }
  if (data->find("data: [DONE]") != std::string::npos) {
    *data = full_content;
    full_content.clear();
    return total_size;
  }
  if (pos > 0)
    data->erase(0, pos);
  // 检查流式结束标志，遇到data: [DONE]时，将完整内容写入data，供ask返回
  return total_size;
}

// 进度回调函数，用于检查非流式请求的中断
int deepseek::ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                              curl_off_t ultotal, curl_off_t ulnow) {
  // 检查是否需要中断请求
  if (g_interrupt_stream.load()) {
    return 1; // 返回非零值中断请求
  }
  return 0; // 继续请求
}

// 非流式传输的写回调函数
size_t deepseek::WriteCallbackNonStream(void *contents, size_t size, size_t nmemb,
                                       std::string *data) {
  // 检查是否需要中断
  if (g_interrupt_stream.load()) {
    return 0; // 中断传输
  }
  
  size_t total_size = size * nmemb;
  data->append((char *)contents, total_size);
  return total_size;
}

deepseek::deepseek(const std::string &key, bool is_stream, HistoryManager* hist_manager)
    : api_key(key), is_stream(is_stream), history_manager(hist_manager) {
  if (key.empty()) {
    throw std::invalid_argument("API key cannot be empty");
  }
}

std::string deepseek::send_request(const std::string &model,
                                   const std::string role,
                                   const std::string &data) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Failed to initialize cURL");
  }
  std::string response_str;
  std::string url = "https://api.deepseek.com/v1/chat/completions";
  // prepare headers
  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers =
      curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
  // create JSON request body
  Json::Value request_body;
  request_body["model"] = model;
  request_body["temperature"] = 0.7;
  if (is_stream) {
    request_body["stream"] = true; // Enable streaming mode
  } else {
    request_body["stream"] = false; // Disable streaming mode
  }
  // add user or tool messages to body
  add_message(role, data);
  request_body["messages"] = messages;
  Json::StreamWriterBuilder writer;
  std::string request_str = Json::writeString(writer, request_body);
  if (request_str.empty()) {
    throw std::runtime_error("Failed to create JSON request body");
  }
#ifdef DEBUG
  std::cout << "Request: " << request_str << std::endl;
#endif
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
  
  // 根据是否流式模式选择不同的回调函数
  if (is_stream) {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  } else {
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallbackNonStream);
    // 为非流式请求设置进度回调以检查中断
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
  }
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
  
  // 设置超时，避免无限等待
  if (is_stream) {
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);  // 流式模式允许更长时间
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  } else {
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);  // 非流式模式30秒超时
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  }
  
  // 重置中断标志
  g_interrupt_stream = false;

  // 发起请求
  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  
  if (res != CURLE_OK) {
    // 检查是否是被中断导致的错误
    if (g_interrupt_stream.load()) {
      return ""; // 静默返回空响应
    }
    throw std::runtime_error("cURL error: " +
                             std::string(curl_easy_strerror(res)));
  }
  
  // 检查是否在传输完成后才被中断
  if (g_interrupt_stream.load() && response_str.empty()) {
    return ""; // 静默返回空响应
  }
  return response_str;
}

std::string deepseek::parseResponse(const std::string &json_response) {
  Json::CharReaderBuilder reader;
  Json::Value root;
  std::string errors;
  std::istringstream json_stream(json_response);
  if (!Json::parseFromStream(reader, json_stream, &root, &errors)) {
    std::cerr << "JSON parse error: " << errors << std::endl;
    return "";
  }
  // 提取回复内容
  if (root.isMember("choices") && root["choices"].isArray() &&
      !root["choices"].empty()) {
    Json::Value choice = root["choices"][0];
    if (choice.isMember("message") && choice["message"].isMember("content")) {
      return choice["message"]["content"].asString();
    }
  }
  return "[Error: Invalid response format]";
}
bool deepseek::add_message(const std::string &role,
                           const std::string &content) {
  if (role.empty() || content.empty()) {
#ifdef DEBUG
    std::cerr << "Invalid message: role or content is empty." << std::endl;
    std::cerr << "Role: '" << role << "', Content: '" << content << "'"
              << std::endl;
#endif
    return false; // Invalid message
  }
  Json::Value message;
  message["role"] = role;
  message["content"] = content;
  messages.append(message);
  return true;
}

std::string deepseek::ask(const std::string &model, const std::string &question,
                          bool multi_turn) {
  std::string jsonresponse;
  std::string response;
  
  if (is_stream) {
    response = send_request(model, "user", question);
  } else {
    // 非流式模式：显示等待提示
    std::cout << "正在思考中..." << std::flush;
    
    jsonresponse = send_request(model, "user", question);
    
    // 检查是否被中断
    if (g_interrupt_stream.load() || jsonresponse.empty()) {
      std::cout << "\r              \r" << std::flush; // 清除"正在思考中..."
      return ""; // 静默返回空响应
    }
    
    response = parseResponse(jsonresponse);
    
    // 清除等待提示
    std::cout << "\r              \r" << std::flush;
    
    if (!response.empty()) {
      std::cout << response << std::endl;
    }
  }
  
  // 保存到历史记录
  if (history_manager && !response.empty()) {
    history_manager->add_entry_multi_turn(question, response, current_system_prompt, model);
  }
  
  if (multi_turn && !response.empty())
    add_message("assistant", response); // Store the assistant's response
  else if (!multi_turn)
    messages.clear(); // Clear messages for single-turn conversations
  return response;
}
bool deepseek::set_system_prompt(const std::string &prompt) noexcept {
  if (prompt.empty()) {
    return false; // Invalid system prompt
  }
  current_system_prompt = prompt; // 保存当前系统提示
  Json::Value system_message;
  // clear previous system message if exists
  for (Json::Value::ArrayIndex i = 0; i < this->messages.size(); ++i) {
    if (this->messages[i]["role"].asString() == "system") {
      this->messages.removeIndex(i, nullptr);
    }
  }
  system_message["role"] = "system";
  system_message["content"] = prompt;
  this->messages.insert(0, system_message); // Insert at the beginning
  return true;
}

void deepseek::set_history_manager(HistoryManager* hist_manager) {
  history_manager = hist_manager;
}

std::string deepseek::get_system_prompt() const {
  return current_system_prompt;
}

std::string deepseek::start_new_session() {
  if (history_manager) {
    return history_manager->start_new_session();
  }
  return "";
}

void deepseek::set_current_session(const std::string& session_id) {
  if (history_manager) {
    history_manager->set_current_session_id(session_id);
  }
}

std::string deepseek::get_current_session_id() const {
  if (history_manager) {
    return history_manager->get_current_session_id();
  }
  return "";
}

void deepseek::load_session_context(const std::string& session_id, int max_turns) {
  if (!history_manager) {
    return;
  }
  
  auto session_entries = history_manager->get_session_history(session_id);
  if (session_entries.empty()) {
    return;
  }
  
  // 清除当前的messages（除了system prompt）
  Json::Value system_message;
  bool has_system = false;
  for (Json::Value::ArrayIndex i = 0; i < messages.size(); ++i) {
    if (messages[i]["role"].asString() == "system") {
      system_message = messages[i];
      has_system = true;
      break;
    }
  }
  
  messages.clear();
  if (has_system) {
    messages.append(system_message);
  }
  
  // 限制加载的轮次数
  int start_index = 0;
  if (max_turns > 0 && static_cast<int>(session_entries.size()) > max_turns) {
    start_index = static_cast<int>(session_entries.size()) - max_turns;
  }
  
  // 按顺序添加历史对话到messages
  for (int i = start_index; i < static_cast<int>(session_entries.size()); ++i) {
    const auto& entry = session_entries[i];
    
    // 添加用户消息
    Json::Value user_msg;
    user_msg["role"] = "user";
    user_msg["content"] = entry.user_message;
    messages.append(user_msg);
    
    // 添加助手回复
    Json::Value assistant_msg;
    assistant_msg["role"] = "assistant";
    assistant_msg["content"] = entry.assistant_response;
    messages.append(assistant_msg);
  }
}

void deepseek::clear_conversation_context() {
  // 保留系统提示，清除其他消息
  Json::Value system_message;
  bool has_system = false;
  for (Json::Value::ArrayIndex i = 0; i < messages.size(); ++i) {
    if (messages[i]["role"].asString() == "system") {
      system_message = messages[i];
      has_system = true;
      break;
    }
  }
  
  messages.clear();
  if (has_system) {
    messages.append(system_message);
  }
}