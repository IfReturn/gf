#include "arg_parser.hpp"
#include <algorithm>
arg_parser::arg_parser(int argc, char** argv) {
    parse(argc, argv);
}
void arg_parser::parse(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (is_option(arg)) {
            options.push_back(arg);
            if (i + 1 < argc && !is_option(argv[i + 1])) {
                options.back() += "=" + extract_option_value(argv[i + 1]);
                ++i; // Skip the value
            }
        } else {
            positional_args.push_back(arg);
        }
    }
}
bool arg_parser::has_option(const std::string& option) const{
    return std::find(options.begin(), options.end(), option) != options.end();
}
std::string arg_parser::get_option_value(const std::string& option) const {
    auto it = std::find_if(options.begin(), options.end(),
                           [&option](const std::string& opt) {
                               return opt.find(option + "=") == 0;
                           });
    if (it != options.end()) {
        return it->substr(option.length() + 1); // Return the value after '='
    }
    return "";
}
std::vector<std::string> arg_parser::get_positional_args() const {
    return positional_args;
}
bool arg_parser::is_option(const std::string& arg) const {
    return !arg.empty() && arg[0] == '-';
}
std::string arg_parser::extract_option_value(const std::string& arg) const {
    size_t pos = arg.find('=');
    if (pos != std::string::npos) {
        return arg.substr(pos + 1);
    }
    return "";
}
arg_parser::~arg_parser() {
    // Destructor can be empty since we are not managing any dynamic memory
}
