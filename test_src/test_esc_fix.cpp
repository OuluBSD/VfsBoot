#include <iostream>
#include <string>

// Mock implementation of the CppString::esc function to test our fix
std::string cpp_string_esc(const std::string& x){
    std::string out;
    out.reserve(x.size()+8);
    auto append_octal = [&out](unsigned char uc){
        out.push_back('\\');
        out.push_back(static_cast<char>('0' + ((uc >> 6) & 0x7)));
        out.push_back(static_cast<char>('0' + ((uc >> 3) & 0x7)));
        out.push_back(static_cast<char>('0' + (uc & 0x7)));
    };

    for(size_t i=0;i<x.size();++i){
        unsigned char uc = static_cast<unsigned char>(x[i]);
        switch(uc){
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\v': out += "\\v"; break;
            case '\a': out += "\\a"; break;
            default:
                if(uc < 0x20 || uc == 0x7f){
                    append_octal(uc);
                } else if(uc >= 0x80){
                    append_octal(uc);
                } else {
                    out.push_back(static_cast<char>(uc));
                }
        }
    }
    return out;
}

int main() {
    // Test the CppString::esc function with actual characters
    std::string test1 = "line-1\nline-2"; // Contains actual newline
    std::string test2 = "tab\tcharacter"; // Contains actual tab
    std::string test3 = "quote\"character"; // Contains actual quote
    std::string test4 = "backslash\\character"; // Contains actual backslash
    
    std::cout << "Test 1 (newline): \"" << cpp_string_esc(test1) << "\"" << std::endl;
    std::cout << "Test 2 (tab): \"" << cpp_string_esc(test2) << "\"" << std::endl;
    std::cout << "Test 3 (quote): \"" << cpp_string_esc(test3) << "\"" << std::endl;
    std::cout << "Test 4 (backslash): \"" << cpp_string_esc(test4) << "\"" << std::endl;
    
    return 0;
}