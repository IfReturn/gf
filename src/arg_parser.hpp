#pragma once
#include <string>
#include <vector>
class arg_parser{
public:
    arg_parser(int argc, char** argv);
    ~arg_parser();

    bool has_option(const std::string& option) const;
    std::string get_option_value(const std::string& option) const;
    std::vector<std::string> get_positional_args() const;
private:
    std::vector<std::string> options;
    std::vector<std::string> positional_args;
    void parse(int argc, char** argv);
    bool is_option(const std::string& arg) const;
    std::string extract_option_value(const std::string& arg) const;
};