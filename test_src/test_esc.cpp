#include <iostream>
#include <string>

// Mock implementation of the esc function for testing
std::string esc(const std::string& x){
    std::string out;
    out.reserve(x.size()*2); // Reserve more space as escaping typically increases size
    
    auto append_octal = [&out](unsigned char uc){
        out.push_back('\\');
        out.push_back(static_cast<char>('0' + ((uc >> 6) & 0x7)));
        out.push_back(static_cast<char>('0' + ((uc >> 3) & 0x7)));
        out.push_back(static_cast<char>('0' + (uc & 0x7)));
    };

    bool escape_next_question = false;
    for(size_t i=0;i<x.size();++i){
        unsigned char uc = static_cast<unsigned char>(x[i]);
        if(uc=='?'){
            if(escape_next_question || (i+1<x.size() && x[i+1]=='?')){
                out += "\\?";
                escape_next_question = (i+1<x.size() && x[i+1]=='?');
            } else {
                out.push_back('?');
                escape_next_question = false;
            }
            continue;
        }

        escape_next_question = false;
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
                } else {
                    out.push_back(static_cast<char>(uc));
                }
        }
    }
    return out;
}

int main() {
    // Test with a string that has actual newline characters
    std::string test1 = "line-1\nline-2";
    std::cout << "Test 1 - Input: \"" << test1 << "\"" << std::endl;
    std::cout << "Test 1 - Escaped: \"" << esc(test1) << "\"" << std::endl;
    
    // Test with a string that has literal backslash-n
    std::string test2 = "line-1\\nline-2";
    std::cout << "Test 2 - Input: \"" << test2 << "\"" << std::endl;
    std::cout << "Test 2 - Escaped: \"" << esc(test2) << "\"" << std::endl;
    
    return 0;
}