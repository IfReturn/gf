#pragma once
#include <curl/curl.h>
#include <json/value.h>
#include <string>
#include <json/json.h>
#include <stdexcept>
#include <iostream>
class deepseek{
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
    deepseek(const std::string& key,bool is_stream = false) : api_key(key),is_stream(is_stream) {
        if (key.empty()) {
            throw std::invalid_argument("API key cannot be empty");
        }
    }
    /**
     * @brief Sends a request to the DeepSeek API and returns the response.
     * @param model The model to use for the request.
     * @param role The role of the message (e.g., "user", "assistant").
     * @param data The content of the message.
     * @return The response from the DeepSeek API as a string(typiclly json type).
     * @throws std::runtime_error if cURL initialization fails or if the request fails.
     */
    std::string send_request(const std::string& model,const std::string role, const std::string& data) {
        CURL* curl = curl_easy_init();/// @todo maybe it will be better to use a singleton pattern for curl
        
        if (!curl) {
            throw std::runtime_error("Failed to initialize cURL");
        }
        std::string response_str;
        std::string url = "https://api.deepseek.com/v1/chat/completions";

        // prepare headers
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
        // create JSON request body
        Json::Value request_body;
        request_body["model"] = model;
        request_body["temperature"] = 0.7;
        // add user or tool messages to body
        add_message(role, data);
        request_body["messages"] = messages;
        Json::StreamWriterBuilder writer;
        std::string request_str = Json::writeString(writer, request_body);
        if (request_str.empty()) {
            throw std::runtime_error("Failed to create JSON request body");
        }
        std::cout << "Request: " << request_str << std::endl;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
        //发起请求
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw std::runtime_error("cURL error: " + std::string(curl_easy_strerror(res)));
        }
        return response_str;
    }
    /**
     * @brief Parses the JSON response from the DeepSeek API and extracts the reply content.
     * @param json_response The JSON response string from the API.
     * @return The reply content as a string, or an error message if parsing fails.
     */
    
    std::string parseResponse(const std::string& json_response) {
        Json::CharReaderBuilder reader;
        Json::Value root;
        std::string errors;
        std::istringstream json_stream(json_response);

        if (!Json::parseFromStream(reader, json_stream, &root, &errors)) {
            std::cerr << "JSON parse error: " << errors << std::endl;
            return "";
        }

        // 提取回复内容
        if (root.isMember("choices") && root["choices"].isArray() && !root["choices"].empty()) {
            Json::Value choice = root["choices"][0];
            if (choice.isMember("message") && choice["message"].isMember("content")) {
                return choice["message"]["content"].asString();
            }
        }
        return "[Error: Invalid response format]";
    }
    /**
    * @brief Adds a message to the conversation history.
    * @param role The role of the message (e.g., "user", "assistant").
    * @param content The content of the message.
    * @return true if the message was added successfully, false if the message is invalid.
    */
    bool add_message(const std::string& role, const std::string& content) {
        if (role.empty() || content.empty()) {
            return false; // Invalid message
        }
        Json::Value message;
        message["role"] = role;
        message["content"] = content;
        messages.append(message);
        return true;
    }
    /**
     * @brief Asks a question to the DeepSeek API and returns the response.
     * @param model The model to use for the request.
     * @param question The question to ask.
     * @param multi_turn Whether to keep the conversation history for multi-turn interactions.
     * @return The response from the DeepSeek API as a string.
     */
    std::string ask(const std::string& model, const std::string& question,bool multi_turn = true) {
        std::string jsonresponse = send_request(model,"user", question);
        std::string response = parseResponse(jsonresponse);
        if(multi_turn)
            add_message("assistant", response); // Store the assistant's response
        else
            messages.clear(); // Clear messages for single-turn conversations  
        return response;
    }
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
        size_t total_size = size * nmemb;
        data->append((char*)contents, total_size);
        return total_size;
    }
};