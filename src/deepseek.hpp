#pragma once
#include <curl/curl.h>
#include <json/json.h>
#include <json/value.h>
#include <string>
class deepseek {
private:
    std::string api_key;
    Json::Value messages{Json::arrayValue};
    bool is_stream;

public:
    /**
     * @brief constructor for deepseek class
     * @param key API key for authentication
     * @param is_stream Whether to use streaming mode (default is false).
     * @throws std::invalid_argument if the API key is empty
     * @note The API key is required for making requests to the DeepSeek API.
     */
  deepseek(const std::string &key, bool is_stream = false);
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
    std::string ask(const std::string& model, const std::string& question,bool multi_turn = true);
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};