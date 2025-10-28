#include "VfsShell.h"

// ====== OpenAI helpers ======
static std::string system_prompt_text(){
    // Check if English-only mode is requested
    const char* english_only = std::getenv("CODEX_ENGLISH_ONLY");
    bool use_english = (english_only && std::string(english_only) == "1");

    // Detect language from environment if not forced to English
    std::string lang_instruction;
    if (use_english) {
        lang_instruction = "\nRespond concisely in English.";
    } else {
        // Detect language from LANG environment variable
        const char* lang_env = std::getenv("LANG");
        bool is_finnish = false;
        if (lang_env) {
            std::string lang_str(lang_env);
            is_finnish = (lang_str.find("fi_") == 0 || lang_str.find("fi.") == 0 ||
                         lang_str.find("finnish") != std::string::npos ||
                         lang_str.find("Finnish") != std::string::npos);
        }
        lang_instruction = is_finnish ? "\nRespond concisely in Finnish."
                                      : "\nRespond concisely in English.";
    }

    return std::string("You are a codex-like assistant embedded in a tiny single-binary IDE.\n") +
           snippets::tool_list() + lang_instruction;
}
std::string build_responses_payload(const std::string& model, const std::string& user_prompt){
    std::string sys = system_prompt_text();
    const char* content_type = "input_text";
    std::string js = std::string("{")+
        "\"model\":\""+json_escape(model)+"\","+
        "\"input\":[{\"role\":\"system\",\"content\":[{\"type\":\""+content_type+"\",\"text\":\""+json_escape(sys)+"\"}]},"+
        "{\"role\":\"user\",\"content\":[{\"type\":\""+content_type+"\",\"text\":\""+json_escape(user_prompt)+"\"}]}]}";
    return js;
}
static std::string build_chat_payload(const std::string& model, const std::string& system_prompt, const std::string& user_prompt){
    return std::string("{") +
        "\"model\":\"" + json_escape(model) + "\"," +
        "\"messages\":[" +
            "{\"role\":\"system\",\"content\":\"" + json_escape(system_prompt) + "\"}," +
            "{\"role\":\"user\",\"content\":\"" + json_escape(user_prompt) + "\"}" +
        "]," +
        "\"temperature\":0.0" +
    "}";
}
static int hex_value(char c){
    if(c>='0' && c<='9') return c - '0';
    if(c>='a' && c<='f') return 10 + (c - 'a');
    if(c>='A' && c<='F') return 10 + (c - 'A');
    return -1;
}
static void append_utf8(std::string& out, uint32_t codepoint){
    auto append_replacement = [&](){ out.append("\xEF\xBF\xBD"); };
    if(codepoint <= 0x7F){
        out.push_back(static_cast<char>(codepoint));
        return;
    }
    if(codepoint <= 0x7FF){
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if(codepoint <= 0xFFFF){
        if(codepoint >= 0xD800 && codepoint <= 0xDFFF){
            append_replacement();
            return;
        }
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if(codepoint <= 0x10FFFF){
        out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    append_replacement();
}
static bool decode_unicode_escape_sequence(const std::string& raw, size_t u_pos, size_t& consumed, uint32_t& codepoint){
    if(u_pos >= raw.size()) return false;
    if(u_pos + 4 >= raw.size()) return false;
    uint32_t code = 0;
    for(int k = 0; k < 4; ++k){
        int v = hex_value(raw[u_pos + 1 + k]);
        if(v < 0) return false;
        code = (code << 4) | static_cast<uint32_t>(v);
    }

    size_t total_consumed = 5; // 'u' + 4 hex digits
    size_t last_digit_pos = u_pos + 4;

    if(code >= 0xD800 && code <= 0xDBFF){
        size_t next_slash = last_digit_pos + 1;
        if(next_slash + 5 < raw.size() && raw[next_slash] == '\\' && raw[next_slash + 1] == 'u'){
            uint32_t low = 0;
            for(int k = 0; k < 4; ++k){
                int v = hex_value(raw[next_slash + 2 + k]);
                if(v < 0) return false;
                low = (low << 4) | static_cast<uint32_t>(v);
            }
            if(low >= 0xDC00 && low <= 0xDFFF){
                code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
                total_consumed += 6; // '\' 'u' + 4 hex digits
            } else {
                code = 0xFFFD;
            }
        } else {
            code = 0xFFFD;
        }
    } else if(code >= 0xDC00 && code <= 0xDFFF){
        code = 0xFFFD;
    }

    consumed = total_consumed;
    codepoint = code;
    return true;
}
static std::optional<std::string> decode_json_string(const std::string& raw, size_t quote_pos){
    if(quote_pos>=raw.size() || raw[quote_pos] != '"') return std::nullopt;
    std::string out;
    bool escape=false;
    for(size_t i = quote_pos + 1; i < raw.size(); ++i){
        char c = raw[i];
        if(escape){
            switch(c){
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'v': out.push_back('\v'); break;
                case 'a': out.push_back('\a'); break;
                case '\\': out.push_back('\\'); break;
                case '"': out.push_back('"'); break;
                case 'u':{
                    size_t consumed = 0;
                    uint32_t codepoint = 0;
                    if(decode_unicode_escape_sequence(raw, i, consumed, codepoint)){
                        append_utf8(out, codepoint);
                        if(consumed > 0) i += consumed - 1;
                    } else {
                        out.push_back('\\');
                        out.push_back('u');
                    }
                    break;
                }
                default:
                    out.push_back(c);
                    break;
            }
            escape=false;
            continue;
        }
        if(c=='\\'){
            escape=true;
            continue;
        }
        if(c=='"') return out;
        out.push_back(c);
    }
    return std::nullopt;
}
static std::optional<std::string> json_string_value_after_colon(const std::string& raw, size_t colon_pos){
    if(colon_pos==std::string::npos) return std::nullopt;
    size_t value_pos = raw.find_first_not_of(" \t\r\n", colon_pos + 1);
    if(value_pos==std::string::npos || raw[value_pos] != '"') return std::nullopt;
    return decode_json_string(raw, value_pos);
}
static std::optional<std::string> find_json_string_field(const std::string& raw, const std::string& field, size_t startPos=0){
    std::string marker = std::string("\"") + field + "\"";
    size_t pos = raw.find(marker, startPos);
    if(pos==std::string::npos) return std::nullopt;
    size_t colon = raw.find(':', pos + marker.size());
    if(colon==std::string::npos) return std::nullopt;
    size_t quote = raw.find('"', colon+1);
    if(quote==std::string::npos) return std::nullopt;
    return decode_json_string(raw, quote);
}
static std::optional<std::string> openai_extract_output_text(const std::string& raw){
    size_t search_pos = 0;
    while(true){
        size_t type_pos = raw.find("\"type\"", search_pos);
        if(type_pos==std::string::npos) break;
        size_t colon = raw.find(':', type_pos);
        if(colon==std::string::npos) break;
        auto type_val = json_string_value_after_colon(raw, colon);
        if(type_val && *type_val == "output_text"){
            size_t text_pos = raw.find("\"text\"", colon);
            while(text_pos!=std::string::npos){
                size_t text_colon = raw.find(':', text_pos);
                if(text_colon==std::string::npos) break;
                if(auto text_val = json_string_value_after_colon(raw, text_colon)) return text_val;
                text_pos = raw.find("\"text\"", text_pos + 6);
            }
        }
        search_pos = colon + 1;
    }

    std::string legacy_marker = "\"output_text\"";
    if(size_t legacy_pos = raw.find(legacy_marker); legacy_pos!=std::string::npos){
        size_t colon = raw.find(':', legacy_pos);
        if(auto legacy_val = json_string_value_after_colon(raw, colon)) return legacy_val;
        if(colon!=std::string::npos){
            size_t quote = raw.find('"', colon);
            if(auto legacy_val = decode_json_string(raw, quote)) return legacy_val;
        }
    }
    return std::nullopt;
}
static std::string build_llama_completion_payload(const std::string& system_prompt, const std::string& user_prompt){
    std::string prompt = std::string("<|system|>\n") + system_prompt + "\n<|user|>\n" + user_prompt + "\n<|assistant|>";
    return std::string("{") +
        "\"prompt\":\"" + json_escape(prompt) + "\"," +
        "\"temperature\":0.0,\"stream\":false" +
    "}";
}
static std::optional<std::string> load_openai_key(){
    if(const char* envKey = std::getenv("OPENAI_API_KEY")){ if(*envKey) return std::string(envKey); }
    const char* home = std::getenv("HOME");
    if(!home || !*home) return std::nullopt;
    std::string path = std::string(home) + "/openai-key.txt";
    std::ifstream f(path, std::ios::binary);
    if(!f) return std::nullopt;
    std::string contents((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    while(!contents.empty() && (contents.back()=='\n' || contents.back()=='\r')) contents.pop_back();
    if(contents.empty()) return std::nullopt;
    return contents;
}

std::string call_openai(const std::string& prompt){
    auto keyOpt = load_openai_key();
    if(!keyOpt) return "error: OPENAI_API_KEY puuttuu ympäristöstä tai ~/openai-key.txt-tiedostosta";
    const std::string& key = *keyOpt;
    std::string base = std::getenv("OPENAI_BASE_URL") ? std::getenv("OPENAI_BASE_URL") : std::string("https://api.openai.com/v1");
    if(base.size() && base.back()=='/') base.pop_back();
    std::string model = std::getenv("OPENAI_MODEL") ? std::getenv("OPENAI_MODEL") : std::string("gpt-4o-mini");

    std::string payload = build_responses_payload(model, prompt);

    bool curl_ok = has_cmd("curl"); bool wget_ok = has_cmd("wget");
    if(!curl_ok && !wget_ok) return "error: curl tai wget ei löydy PATHista";

    std::string tmp = "/tmp/oai_req_" + std::to_string(::getpid()) + ".json";
    { std::ofstream f(tmp, std::ios::binary); if(!f) return "error: ei voi avata temp-tiedostoa"; f.write(payload.data(), (std::streamsize)payload.size()); }

    std::string cmd;
    if(curl_ok){
        cmd = std::string("curl -sS -X POST ")+base+"/responses -H 'Content-Type: application/json' -H 'Authorization: Bearer "+key+"' --data-binary @"+tmp;
    } else {
        cmd = std::string("wget -qO- --method=POST --header=Content-Type:application/json ")+
              std::string("--header=Authorization:'Bearer ")+key+"' "+base+"/responses --body-file="+tmp;
    }

    std::string raw = exec_capture(cmd, "ai:openai");
    std::remove(tmp.c_str());
    if(raw.empty()) return "error: tyhjä vastaus OpenAI:lta\n";

    // very crude output_text extraction
    if(auto text = openai_extract_output_text(raw)){
        return std::string("AI: ") + *text + "\n";
    }
    return raw + "\n";
}

std::string call_llama(const std::string& prompt){
    auto env_or_empty = [](const char* name) -> std::string {
        if(const char* val = std::getenv(name); val && *val) return std::string(val);
        return {};
    };
    std::string base = env_or_empty("LLAMA_BASE_URL");
    if(base.empty()) base = env_or_empty("LLAMA_SERVER");
    if(base.empty()) base = env_or_empty("LLAMA_URL");
    if(base.empty()) base = "http://192.168.1.169:8080";
    if(!base.empty() && base.back()=='/') base.pop_back();

    std::string model = env_or_empty("LLAMA_MODEL");
    if(model.empty()) model = "coder";

    bool curl_ok = has_cmd("curl"); bool wget_ok = has_cmd("wget");
    if(!curl_ok && !wget_ok) return "error: curl tai wget ei löydy PATHista";

    std::string system_prompt = system_prompt_text();

    static unsigned long long llama_req_counter = 0;
    auto send_request = [&](const std::string& endpoint, const std::string& payload) -> std::string {
        std::string tmp = "/tmp/llama_req_" + std::to_string(::getpid()) + "_" + std::to_string(++llama_req_counter) + ".json";
        {
            std::ofstream f(tmp, std::ios::binary);
            if(!f) return std::string();
            f.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }
        std::string url = base + endpoint;
        std::string cmd;
        if(curl_ok){
            cmd = std::string("curl -sS -X POST \"") + url + "\" -H \"Content-Type: application/json\" --data-binary @" + tmp;
        } else {
            cmd = std::string("wget -qO- --method=POST --header=Content-Type:application/json --body-file=") + tmp + " \"" + url + "\"";
        }
        std::string raw = exec_capture(cmd, std::string("ai:llama ")+endpoint);
        std::remove(tmp.c_str());
        return raw;
    };

    auto parse_chat_response = [&](const std::string& raw) -> std::optional<std::string> {
        if(raw.empty()) return std::nullopt;
        if(auto err = find_json_string_field(raw, "error")) return std::string("error: llama: ") + *err;
        size_t assistantPos = raw.find("\"role\":\"assistant\"");
        size_t searchPos = assistantPos==std::string::npos ? 0 : assistantPos;
        if(auto content = find_json_string_field(raw, "content", searchPos)) return std::string("AI: ") + *content;
        if(auto text = find_json_string_field(raw, "text", searchPos)) return std::string("AI: ") + *text;
        if(auto generic = find_json_string_field(raw, "result")) return std::string("AI: ") + *generic;
        return std::nullopt;
    };

    std::string chat_payload = build_chat_payload(model, system_prompt, prompt);
    std::string chat_raw = send_request("/v1/chat/completions", chat_payload);
    if(auto parsed = parse_chat_response(chat_raw)){
        if(parsed->rfind("error:", 0)==0) return *parsed + "\n";
        return *parsed + "\n";
    }

    std::string comp_payload = build_llama_completion_payload(system_prompt, prompt);
    std::string comp_raw = send_request("/completion", comp_payload);
    if(comp_raw.empty()){
        if(!chat_raw.empty()) return std::string("error: llama: unexpected response: ") + chat_raw + "\n";
        return "error: tyhjä vastaus llama-palvelimelta\n";
    }
    if(auto err = find_json_string_field(comp_raw, "error")) return std::string("error: llama: ") + *err + "\n";
    if(auto completion = find_json_string_field(comp_raw, "completion")) return std::string("AI: ") + *completion + "\n";
    size_t choicesPos = comp_raw.find("\"choices\"");
    if(auto text = find_json_string_field(comp_raw, "text", choicesPos==std::string::npos ? 0 : choicesPos))
        return std::string("AI: ") + *text + "\n";
    return std::string("error: llama: unexpected response: ") + comp_raw + "\n";
}

static bool env_truthy(const char* name){
    if(const char* val = std::getenv(name)) return *val != '\0';
    return false;
}

static std::string env_string(const char* name){
    if(const char* val = std::getenv(name); val && *val) return std::string(val);
    return {};
}

static std::string openai_cache_signature(){
    std::string base = env_string("OPENAI_BASE_URL");
    if(base.empty()) base = "https://api.openai.com/v1";
    if(!base.empty() && base.back()=='/') base.pop_back();
    std::string model = env_string("OPENAI_MODEL");
    if(model.empty()) model = "gpt-4o-mini";
    return std::string("openai|") + model + '|' + base;
}

static std::string llama_cache_signature(){
    std::string base = env_string("LLAMA_BASE_URL");
    if(base.empty()) base = env_string("LLAMA_SERVER");
    if(base.empty()) base = env_string("LLAMA_URL");
    if(base.empty()) base = "http://192.168.1.169:8080";
    if(!base.empty() && base.back()=='/') base.pop_back();
    std::string model = env_string("LLAMA_MODEL");
    if(model.empty()) model = "coder";
    return std::string("llama|") + model + '|' + base;
}

// AI cache implementation
static std::filesystem::path ai_cache_root(){
    if(const char* home = std::getenv("HOME"); home && *home){
        return std::filesystem::path(home) / ".cache" / "codex" / "ai";
    }
    return std::filesystem::path("cache") / "ai";
}

static std::filesystem::path ai_cache_base_path(const std::string& providerLabel, const std::string& key_material){
    std::filesystem::path dir = ai_cache_root() / sanitize_component(providerLabel);
    std::string hash = hash_hex(fnv1a64(key_material));
    return dir / hash;
}

static std::filesystem::path ai_cache_output_path(const std::string& providerLabel, const std::string& key_material){
    auto base = ai_cache_base_path(providerLabel, key_material);
    base += "-out.txt";
    return base;
}

static std::filesystem::path ai_cache_input_path(const std::string& providerLabel, const std::string& key_material){
    auto base = ai_cache_base_path(providerLabel, key_material);
    base += "-in.txt";
    return base;
}

static std::filesystem::path ai_cache_legacy_output_path(const std::string& providerLabel, const std::string& key_material){
    std::filesystem::path dir = ai_cache_root() / sanitize_component(providerLabel);
    std::string hash = hash_hex(fnv1a64(key_material));
    return dir / (hash + ".txt");
}

static std::string make_cache_key_material(const std::string& providerSignature, const std::string& prompt){
    return providerSignature + '\x1f' + prompt;
}

static std::optional<std::string> ai_cache_read(const std::string& providerLabel, const std::string& key_material){
    auto out_path = ai_cache_output_path(providerLabel, key_material);
    std::ifstream in(out_path, std::ios::binary);
    if(!in){
        auto legacy = ai_cache_legacy_output_path(providerLabel, key_material);
        in.open(legacy, std::ios::binary);
        if(!in) return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static void ai_cache_write(const std::string& providerLabel, const std::string& key_material, const std::string& prompt, const std::string& payload){
    auto out_path = ai_cache_output_path(providerLabel, key_material);
    auto in_path = ai_cache_input_path(providerLabel, key_material);
    std::error_code ec;
    std::filesystem::create_directories(out_path.parent_path(), ec);
    {
        std::ofstream in_file(in_path, std::ios::binary);
        if(in_file) in_file.write(prompt.data(), static_cast<std::streamsize>(prompt.size()));
    }
    std::ofstream out(out_path, std::ios::binary);
    if(!out) return;
    out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

std::string call_ai(const std::string& prompt){
    auto dispatch_with_cache = [&](const std::string& providerLabel, const std::string& signature, auto&& fn) -> std::string {
        std::string key_material = make_cache_key_material(signature, prompt);
        if(auto cached = ai_cache_read(providerLabel, key_material)){
            return *cached;
        }
        std::string response = fn();
        ai_cache_write(providerLabel, key_material, prompt, response);
        return response;
    };

    auto use_llama = [&]() -> std::string {
        const std::string signature = llama_cache_signature();
        return dispatch_with_cache("llama", signature, [&](){ return call_llama(prompt); });
    };

    auto use_openai = [&]() -> std::string {
        const std::string signature = openai_cache_signature();
        return dispatch_with_cache("openai", signature, [&](){ return call_openai(prompt); });
    };

    std::string provider;
    if(const char* p = std::getenv("CODEX_AI_PROVIDER")) provider = p;
    std::transform(provider.begin(), provider.end(), provider.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    if(provider == "llama") return use_llama();
    if(provider == "openai") return use_openai();

    bool llama_hint = env_truthy("LLAMA_BASE_URL") || env_truthy("LLAMA_SERVER") || env_truthy("LLAMA_URL");
    auto keyOpt = load_openai_key();

    if(!keyOpt){
        return use_llama();
    }

    if(llama_hint) return use_llama();
    return use_openai();
}

//

//
// BLAKE3 hash functions
//
std::string compute_string_hash(const std::string& data){
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data.data(), data.size());
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for(size_t i = 0; i < BLAKE3_OUT_LEN; ++i){
        oss << std::setw(2) << static_cast<unsigned>(output[i]);
    }
    return oss.str();
}

std::string compute_file_hash(const std::string& filepath){
    std::ifstream file(filepath, std::ios::binary);
    if(!file) throw std::runtime_error("cannot open file for hashing: " + filepath);

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    constexpr size_t BUFFER_SIZE = 65536;
    std::vector<char> buffer(BUFFER_SIZE);
    while(file.read(buffer.data(), BUFFER_SIZE) || file.gcount() > 0){
        blake3_hasher_update(&hasher, buffer.data(), static_cast<size_t>(file.gcount()));
    }

    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for(size_t i = 0; i < BLAKE3_OUT_LEN; ++i){
        oss << std::setw(2) << static_cast<unsigned>(output[i]);
    }
    return oss.str();
}

