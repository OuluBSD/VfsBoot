#include <iostream>
#include <string>
#include <vector>

// Mock implementation of join_args for testing
std::string join_args(const std::vector<std::string>& args, size_t start = 0){
    std::string out;
    for(size_t i = start; i < args.size(); ++i){
        if(i > start) out.push_back(' ');
        out += args[i];
    }
    return out;
}

// Mock implementation of unescape_meta for testing
std::string unescape_meta(const std::string& s){
    std::cout << "unescape_meta input: \"" << s << "\"" << std::endl;
    std::string out; out.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        char c = s[i];
        if(c=='\\' && i+1<s.size()){
            char n = s[++i];
            switch(n){
                case 'n': out.push_back('\n'); std::cout << "Converted \\\\n to newline" << std::endl; break;
                case 't': out.push_back('\t'); std::cout << "Converted \\\\t to tab" << std::endl; break;
                case 'r': out.push_back('\r'); std::cout << "Converted \\\\r to carriage return" << std::endl; break;
                case '\\': out.push_back('\\'); std::cout << "Converted \\\\\\\\ to backslash" << std::endl; break;
                case '"': out.push_back('"'); std::cout << "Converted \\\\\" to quote" << std::endl; break;
                case 'b': out.push_back('\b'); std::cout << "Converted \\\\b to backspace" << std::endl; break;
                case 'f': out.push_back('\f'); std::cout << "Converted \\\\f to form feed" << std::endl; break;
                case 'v': out.push_back('\v'); std::cout << "Converted \\\\v to vertical tab" << std::endl; break;
                case 'a': out.push_back('\a'); std::cout << "Converted \\\\a to alert" << std::endl; break;
                default: out.push_back(n); std::cout << "Unknown escape sequence: \\\\" << n << std::endl; break;
            }
        } else {
            out.push_back(c);
        }
    }
    std::cout << "unescape_meta output: \"" << out << "\"" << std::endl;
    return out;
}

int main() {
    // Simulate the arguments that would be passed to cpp.print
    std::vector<std::string> args = {
        "/astcpp/escape/main",
        "line-1\\nline-2",
        "\\t\\\"quote\\\"",
        "backslash",
        "\\\\",
        "question??/",
        "done"
    };
    
    std::string text = unescape_meta(join_args(args, 1));
    std::cout << "Final text: \"" << text << "\"" << std::endl;
    
    return 0;
}