#include "upp_builder.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

std::string trim_copy(const std::string& input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if(begin == std::string::npos) return {};
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::string to_upper_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return value;
}

bool parse_bm_line(const std::string& line,
                   std::string& key_out,
                   std::string& value_out,
                   std::string& error_out) {
    key_out.clear();
    value_out.clear();
    error_out.clear();

    std::string trimmed = trim_copy(line);
    if(trimmed.empty()) return true;
    if(trimmed.rfind("//", 0) == 0) return true;

    // Remove inline // comments unless inside a quoted segment
    bool in_quotes = false;
    bool escape = false;
    std::string without_comment;
    for(size_t i = 0; i < trimmed.size(); ++i) {
        char ch = trimmed[i];
        if(in_quotes) {
            if(escape) {
                without_comment.push_back(ch);
                escape = false;
            } else if(ch == '\\') {
                without_comment.push_back(ch);
                escape = true;
            } else if(ch == '"') {
                in_quotes = false;
                without_comment.push_back(ch);
            } else {
                without_comment.push_back(ch);
            }
        } else {
            if(ch == '"') {
                in_quotes = true;
                without_comment.push_back(ch);
            } else if(ch == '/' && i + 1 < trimmed.size() && trimmed[i + 1] == '/') {
                break;
            } else {
                without_comment.push_back(ch);
            }
        }
    }

    trimmed = trim_copy(without_comment);
    if(trimmed.empty()) return true;

    auto eq_pos = trimmed.find('=');
    if(eq_pos == std::string::npos) {
        // Be permissive: ignore lines without assignments (e.g. legacy constructs)
        return true;
    }

    std::string key = trim_copy(trimmed.substr(0, eq_pos));
    if(key.empty()) {
        error_out = "empty key";
        return false;
    }

    std::string rest = trim_copy(trimmed.substr(eq_pos + 1));
    if(rest.empty()) {
        key_out = to_upper_copy(key);
        value_out.clear();
        return true;
    }

    std::string value;
    size_t idx = 0;
    if(rest[idx] == '"') {
        ++idx;
        bool local_escape = false;
        while(idx < rest.size()) {
            char ch = rest[idx];
            if(local_escape) {
                switch(ch) {
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    case '\\': value.push_back('\\'); break;
                    case '"': value.push_back('"'); break;
                    default: value.push_back(ch); break;
                }
                local_escape = false;
            } else if(ch == '\\') {
                local_escape = true;
            } else if(ch == '"') {
                ++idx;
                break;
            } else {
                value.push_back(ch);
            }
            ++idx;
        }
        if(local_escape) {
            error_out = "unterminated escape sequence in string literal";
            return false;
        }
        if(idx > rest.size()) {
            error_out = "unterminated string literal";
            return false;
        }
        // Consume trailing whitespace and optional semicolon
        while(idx < rest.size() && std::isspace(static_cast<unsigned char>(rest[idx]))) ++idx;
        if(idx < rest.size() && rest[idx] == ';') ++idx;
        while(idx < rest.size() && std::isspace(static_cast<unsigned char>(rest[idx]))) ++idx;
        if(idx < rest.size()) {
            error_out = "unexpected characters after value";
            return false;
        }
    } else {
        // Unquoted value: read until ';'
        size_t semicolon = rest.find(';');
        std::string raw = semicolon == std::string::npos ? rest : rest.substr(0, semicolon);
        value = trim_copy(raw);
    }

    key_out = to_upper_copy(key);
    value_out = value;
    return true;
}

} // namespace

void UppBuildMethod::set(const std::string& key, const std::string& value) {
    std::string normalized = to_upper_copy(key);
    properties[normalized] = value;
    if(normalized == "BUILDER") {
        builder_type = value;
    }
}

std::optional<std::string> UppBuildMethod::get(const std::string& key) const {
    std::string normalized = to_upper_copy(key);
    auto it = properties.find(normalized);
    if(it == properties.end()) return std::nullopt;
    return it->second;
}

bool UppBuildMethod::has(const std::string& key) const {
    std::string normalized = to_upper_copy(key);
    return properties.find(normalized) != properties.end();
}

std::vector<std::string> UppBuildMethod::keys() const {
    std::vector<std::string> result;
    result.reserve(properties.size());
    for(const auto& [key, _] : properties) {
        result.push_back(key);
    }
    return result;
}

std::vector<std::string> UppBuildMethod::splitList(const std::string& key, char delimiter) const {
    std::vector<std::string> result;
    auto value = get(key);
    if(!value) return result;

    std::string current;
    std::istringstream stream(*value);
    while(std::getline(stream, current, delimiter)) {
        current = trim_copy(current);
        if(!current.empty()) result.push_back(current);
    }
    return result;
}

bool UppBuilderRegistry::parseAndStore(const std::string& id,
                                       const std::string& source_path,
                                       const std::string& content,
                                       std::string& error_message) {
    error_message.clear();

    UppBuildMethod method;
    method.id = id;
    method.source_path = source_path;

    std::istringstream stream(content);
    std::string line;
    size_t line_no = 0;
    while(std::getline(stream, line)) {
        ++line_no;
        std::string key;
        std::string value;
        std::string parse_error;
        if(!parse_bm_line(line, key, value, parse_error)) {
            error_message = "line " + std::to_string(line_no) + ": " + parse_error;
            return false;
        }
        if(key.empty()) continue;
        method.set(key, value);
    }

    if(method.builder_type.empty()) {
        auto from_key = method.get("BUILDER");
        if(from_key) {
            method.builder_type = *from_key;
        } else {
            method.builder_type = id;
        }
    }

    methods_[id] = std::move(method);
    if(active_.empty()) {
        active_ = id;
    }
    return true;
}

bool UppBuilderRegistry::has(const std::string& id) const {
    return methods_.find(id) != methods_.end();
}

const UppBuildMethod* UppBuilderRegistry::get(const std::string& id) const {
    auto it = methods_.find(id);
    if(it == methods_.end()) return nullptr;
    return &it->second;
}

UppBuildMethod* UppBuilderRegistry::getMutable(const std::string& id) {
    auto it = methods_.find(id);
    if(it == methods_.end()) return nullptr;
    return &it->second;
}

std::vector<std::string> UppBuilderRegistry::list() const {
    std::vector<std::string> names;
    names.reserve(methods_.size());
    for(const auto& [key, _] : methods_) {
        names.push_back(key);
    }
    return names;
}

bool UppBuilderRegistry::setActive(const std::string& id, std::string& error_message) {
    if(!has(id)) {
        error_message = "builder not found: " + id;
        return false;
    }
    active_ = id;
    error_message.clear();
    return true;
}

std::optional<std::string> UppBuilderRegistry::activeName() const {
    if(active_.empty()) return std::nullopt;
    if(!has(active_)) return std::nullopt;
    return active_;
}

UppBuildMethod* UppBuilderRegistry::active() {
    if(active_.empty()) return nullptr;
    return getMutable(active_);
}

const UppBuildMethod* UppBuilderRegistry::active() const {
    if(active_.empty()) return nullptr;
    return get(active_);
}
