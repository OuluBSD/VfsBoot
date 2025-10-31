#include <iostream>
#include <string>

// Mock implementation of unescape_meta for testing
std::string unescape_meta(const std::string& s){
    std::string out; out.reserve(s.size());
    for(size_t i=0;i<s.size();++i){
        char c = s[i];
        if(c=='\\' && i+1<s.size()){
            char n = s[++i];
            switch(n){
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'v': out.push_back('\v'); break;
                case 'a': out.push_back('\a'); break;
                default: out.push_back(n); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return out;
}

int main() {
    // Test the specific string from the test case
    std::string input = "line-1\\nline-2 \\t\\\"quote\\\" backslash \\\\ question??/ done";
    std::cout << "Input: \"" << input << "\"" << std::endl;
    std::string unescaped = unescape_meta(input);
    std::cout << "Unescaped: \"" << unescaped << "\"" << std::endl;
    
    // Print each character and its ASCII value to see what's actually in the string
    std::cout << "Character analysis:" << std::endl;
    for (size_t i = 0; i < unescaped.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(unescaped[i]);
        std::cout << "  [" << i << "] = " << static_cast<int>(c) << " ('" << unescaped[i] << "')" << std::endl;
    }
    
    return 0;
}