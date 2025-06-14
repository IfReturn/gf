#pragma once
#include <curl/curl.h>
#include <json/json.h>
#include <json/value.h>
#include <string>
#include <atomic>
#include "history.hpp"
#include "global_manager.hpp"

class deepseek {
private:
  std::string api_key;
  Json::Value messages{Json::arrayValue};
  bool is_stream;
  std::string current_system_prompt;
  HistoryManager* history_manager; // 历史记录管理器指针
  std::string current_session_id; // 当前会话ID

public:
  /**
   * @brief constructor for deepseek class
   * @param key API key for authentication
   * @param is_stream Whether to use streaming mode (default is false).
   * @param hist_manager Pointer to history manager (optional).
   * @throws std::invalid_argument if the API key is empty
   * @note The API key is required for making requests to the DeepSeek API.
   */
  deepseek(const std::string &key, bool is_stream = false, HistoryManager* hist_manager = nullptr);
  /**
   * @brief Sends a request to the DeepSeek API and returns the response.
   * @param model The model to use for the request.
   * @param role The role of the message (e.g., "user", "assistant").
   * @param data The content of the message.
   * @return The response from the DeepSeek API as a string(typiclly json type).
   * @throws std::runtime_error if cURL initialization fails or if the request
   * fails.
   */
  std::string send_request(const std::string &model, const std::string role,
                           const std::string &data);
  /**
   * @brief Parses the JSON response from the DeepSeek API and extracts the
   * reply content.
   * @param json_response The JSON response string from the API.
   * @return The reply content as a string, or an error message if parsing
   * fails.
   */
  std::string parseResponse(const std::string &json_response);
  /**
   * @brief Adds a message to the conversation history.
   * @param role The role of the message (e.g., "user", "assistant").
   * @param content The content of the message.
   * @return true if the message was added successfully, false if the message is
   * invalid.
   */
  bool add_message(const std::string &role, const std::string &content);
  /**
   * @brief Asks a question to the DeepSeek API and returns the response.
   * @param model The model to use for the request.
   * @param question The question to ask.
   * @param multi_turn Whether to keep the conversation history for multi-turn
   * interactions.
   * @return The response from the DeepSeek API as a string.
   */
  std::string ask(const std::string &model, const std::string &question,
                  bool multi_turn = true);

  /**
   * @brief set the system prompt for the conversation.
   * @param prompt The system prompt to set.
   * @return true if the system prompt was set successfully, false if the prompt
   * is invalid.
   */
  bool set_system_prompt(const std::string &prompt) noexcept;

  /**
   * @brief Set the history manager for this deepseek instance
   * @param hist_manager Pointer to history manager
   */
  void set_history_manager(HistoryManager* hist_manager);

  /**
   * @brief Get the current system prompt
   * @return Current system prompt
   */
  std::string get_system_prompt() const;

  /**
   * @brief Start a new conversation session
   * @return New session ID
   */
  std::string start_new_session();

  /**
   * @brief Set current session for multi-turn conversation
   * @param session_id Session ID to continue
   */
  void set_current_session(const std::string& session_id);

  /**
   * @brief Get current session ID
   * @return Current session ID
   */
  std::string get_current_session_id() const;

  /**
   * @brief Load conversation context from session history
   * @param session_id Session ID to load
   * @param max_turns Maximum number of turns to load (0 = all)
   */
  void load_session_context(const std::string& session_id, int max_turns = 0);

  /**
   * @brief Clear current conversation context
   */
  void clear_conversation_context();

  /**
   * @brief Extracts the content from a streaming response line.
   * @param line The line from the streaming response.
   * @note the function will output the content to stdout if in stream mode.
   * @return The extracted content as a string.
   */
  static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                              std::string *data);
  
  /**
   * @brief Progress callback for non-streaming requests to check for interruption
   * @param clientp Client data (unused)
   * @param dltotal Total download size
   * @param dlnow Current download size
   * @param ultotal Total upload size  
   * @param ulnow Current upload size
   * @return 0 to continue, non-zero to abort
   */
  static int ProgressCallback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow);
  
  /**
   * @brief Write callback specifically for non-streaming responses
   * @param contents Data received
   * @param size Size of each element
   * @param nmemb Number of elements
   * @param data String to append data to
   * @return Number of bytes processed
   */
  static size_t WriteCallbackNonStream(void *contents, size_t size, size_t nmemb,
                                      std::string *data);
};