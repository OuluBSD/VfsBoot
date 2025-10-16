#include "VfsShell.h"

std::optional<std::filesystem::path> history_file_path(){
    if(const char* env = std::getenv("CODEX_HISTORY_FILE"); env && *env){
        return std::filesystem::path(env);
    }
    if(const char* home = std::getenv("HOME"); home && *home){
        return std::filesystem::path(home) / ".codex_history";
    }
    return std::nullopt;
}

void load_history(std::vector<std::string>& history){
    auto path_opt = history_file_path();
    if(!path_opt) return;
    std::ifstream in(*path_opt);
    if(!in) return;
    std::string line;
    while(std::getline(in, line)){
        if(trim_copy(line).empty()) continue;
        history.push_back(line);
    }
}

bool terminal_available(){
    return ::isatty(STDIN_FILENO) == 1 && ::isatty(STDOUT_FILENO) == 1;
}

void redraw_prompt_line(const std::string& prompt, const std::string& buffer, size_t cursor){
    std::cout << '\r' << prompt << buffer << "\x1b[K";
    if(cursor < buffer.size()){
        size_t tail = buffer.size() - cursor;
        std::cout << "\x1b[" << tail << 'D';
    }
    std::cout.flush();
}

bool read_line_with_history(Vfs& vfs, const std::string& prompt, std::string& out, const std::vector<std::string>& history, const std::string& cwd_path){
    std::cout << prompt;
    std::cout.flush();

    if(!terminal_available()){
        if(!std::getline(std::cin, out)){
            return false;
        }
        return true;
    }

    RawTerminalMode guard;
    if(!guard.ok()){
        if(!std::getline(std::cin, out)){
            return false;
        }
        return true;
    }

    std::string buffer;
    size_t cursor = 0;
    size_t history_pos = history.size();
    std::string saved_new_entry;
    bool saved_valid = false;

    auto redraw_current = [&](){
        redraw_prompt_line(prompt, buffer, cursor);
    };

    auto trigger_save_shortcut = [&](){
        if(!g_on_save_shortcut) return;
        std::cout << '\r';
        std::cout.flush();
        std::cout << '\n';
        g_on_save_shortcut();
        redraw_current();
    };

    while(true){
        unsigned char ch = 0;
        ssize_t n = ::read(STDIN_FILENO, &ch, 1);
        if(n <= 0){
            std::cout << '\n';
            return false;
        }

        if(ch == '\r' || ch == '\n'){
            std::cout << '\n';
            out = buffer;
            return true;
        }

        if(ch == 3){ // Ctrl-C
            std::cout << "^C\n";
            buffer.clear();
            cursor = 0;
            history_pos = history.size();
            saved_valid = false;
            std::cout << prompt;
            std::cout.flush();
            continue;
        }

        if(ch == 4){ // Ctrl-D
            if(buffer.empty()){
                std::cout << '\n';
                return false;
            }
            if(cursor < buffer.size()){
                auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor);
                buffer.erase(pos);
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 9){ // Tab - auto-complete
            bool show_list = false;
            std::string completed = complete_input(vfs, buffer, cursor, cwd_path, show_list);
            if(completed != buffer){
                buffer = completed;
                cursor = buffer.size();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            if(show_list){
                std::cout << prompt;
            }
            redraw_current();
            continue;
        }

        if(ch == 127 || ch == 8){ // backspace
            if(cursor > 0){
                auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor) - 1;
                buffer.erase(pos);
                --cursor;
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 1){ // Ctrl-A
            if(cursor != 0){
                cursor = 0;
                redraw_current();
            }
            continue;
        }

        if(ch == 5){ // Ctrl-E
            if(cursor != buffer.size()){
                cursor = buffer.size();
                redraw_current();
            }
            continue;
        }

        if(ch == 21){ // Ctrl-U
            if(cursor > 0){
                buffer.erase(0, cursor);
                cursor = 0;
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 11){ // Ctrl-K
            if(cursor < buffer.size()){
                buffer.erase(cursor);
                redraw_current();
                if(history_pos != history.size()){
                    history_pos = history.size();
                    saved_valid = false;
                }
            }
            continue;
        }

        if(ch == 27){ // escape sequences
            unsigned char seq1 = 0;
            if(::read(STDIN_FILENO, &seq1, 1) <= 0) continue;
            if(seq1 == 'O'){
                unsigned char seq2 = 0;
                if(::read(STDIN_FILENO, &seq2, 1) <= 0) continue;
                if(seq2 == 'R'){ // F3 shortcut
                    trigger_save_shortcut();
                }
                continue;
            }
            if(seq1 != '['){
                continue;
            }
            unsigned char seq2 = 0;
            if(::read(STDIN_FILENO, &seq2, 1) <= 0) continue;

            if(seq2 >= '0' && seq2 <= '9'){
                unsigned char seq3 = 0;
                if(::read(STDIN_FILENO, &seq3, 1) <= 0) continue;
                if(seq2 == '1' && seq3 == '3'){ // expect ~ for F3
                    unsigned char seq4 = 0;
                    if(::read(STDIN_FILENO, &seq4, 1) <= 0) continue;
                    if(seq4 == '~'){
                        trigger_save_shortcut();
                    }
                    continue;
                }
                if(seq2 == '3' && seq3 == '~'){ // delete key
                    if(cursor < buffer.size()){
                        auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor);
                        buffer.erase(pos);
                        redraw_current();
                        if(history_pos != history.size()){
                            history_pos = history.size();
                            saved_valid = false;
                        }
                    }
                }
                continue;
            }

            if(seq2 == 'A'){ // up
                if(history.empty()){
                    std::cout << '\a' << std::flush;
                    continue;
                }
                if(history_pos == history.size()){
                    if(!saved_valid){
                        saved_new_entry = buffer;
                        saved_valid = true;
                    }
                    history_pos = history.empty() ? 0 : history.size() - 1;
                } else if(history_pos > 0){
                    --history_pos;
                } else {
                    std::cout << '\a' << std::flush;
                    continue;
                }
                buffer = history[history_pos];
                cursor = buffer.size();
                redraw_current();
                continue;
            }

            if(seq2 == 'B'){ // down
                if(history_pos == history.size()){
                    if(saved_valid){
                        buffer = saved_new_entry;
                        cursor = buffer.size();
                        redraw_current();
                        saved_valid = false;
                    } else {
                        std::cout << '\a' << std::flush;
                    }
                    continue;
                }
                ++history_pos;
                if(history_pos == history.size()){
                    buffer = saved_valid ? saved_new_entry : std::string{};
                    cursor = buffer.size();
                    redraw_current();
                saved_valid = false;
                } else {
                    buffer = history[history_pos];
                    cursor = buffer.size();
                    redraw_current();
                }
                continue;
            }

            if(seq2 == 'C'){ // right
                if(cursor < buffer.size()){
                    ++cursor;
                    redraw_current();
                }
                continue;
            }

            if(seq2 == 'D'){ // left
                if(cursor > 0){
                    --cursor;
                    redraw_current();
                }
                continue;
            }

            continue;
        }

        if(ch >= 32 && ch <= 126){
            auto pos = buffer.begin() + static_cast<std::string::difference_type>(cursor);
            buffer.insert(pos, static_cast<char>(ch));
            ++cursor;
            redraw_current();
            if(history_pos != history.size()){
                history_pos = history.size();
                saved_valid = false;
            }
            continue;
        }
    }
}

std::vector<std::string> tokenize_command_line(const std::string& line){
    std::vector<std::string> tokens;
    std::string cur;
    bool in_single = false;
    bool in_double = false;
    bool escape = false;
    auto flush = [&]{
        if(!cur.empty()){
            tokens.push_back(cur);
            cur.clear();
        }
    };
    for(size_t i = 0; i < line.size(); ++i){
        char c = line[i];
        if(escape){
            cur.push_back(c);
            escape = false;
            continue;
        }
        if(!in_single && c == '\\'){
            escape = true;
            continue;
        }
        if(c == '"' && !in_single){
            in_double = !in_double;
            continue;
        }
        if(c == '\'' && !in_double){
            in_single = !in_single;
            continue;
        }
        if(!in_single && !in_double){
            if(std::isspace(static_cast<unsigned char>(c))){
                flush();
                continue;
            }
            if(c == '|'){
                flush();
                if(i + 1 < line.size() && line[i+1] == '|'){
                    tokens.emplace_back("||");
                    ++i;
                } else {
                    tokens.emplace_back("|");
                }
                continue;
            }
            if(c == '&' && i + 1 < line.size() && line[i+1] == '&'){
                flush();
                tokens.emplace_back("&&");
                ++i;
                continue;
            }
            if(c == '>'){
                flush();
                if(i + 1 < line.size() && line[i+1] == '>'){
                    tokens.emplace_back(">>");
                    ++i;
                } else {
                    tokens.emplace_back(">");
                }
                continue;
            }
        }
        cur.push_back(c);
    }
    if(escape) throw std::runtime_error("line ended with unfinished escape");
    if(in_single || in_double) throw std::runtime_error("unterminated quote");
    flush();
    return tokens;
}

std::vector<CommandChainEntry> parse_command_chain(const std::vector<std::string>& tokens){
    std::vector<CommandChainEntry> chain;
    CommandPipeline current_pipe;
    CommandInvocation current_cmd;
    std::string next_logic;

    auto flush_command = [&](){
        if(current_cmd.name.empty()) throw std::runtime_error("expected command before operator");
        current_pipe.commands.push_back(current_cmd);
        current_cmd = CommandInvocation{};
    };

    auto flush_pipeline = [&](){
        if(current_pipe.commands.empty()) throw std::runtime_error("missing command sequence");
        chain.push_back(CommandChainEntry{next_logic, current_pipe});
        current_pipe = CommandPipeline{};
        next_logic.clear();
    };

    for(size_t idx = 0; idx < tokens.size(); ++idx){
        const auto& tok = tokens[idx];
        if(tok == "|"){
            flush_command();
            continue;
        }
        if(tok == "&&" || tok == "||"){
            flush_command();
            flush_pipeline();
            next_logic = tok;
            continue;
        }
        if(tok == ">" || tok == ">>"){
            flush_command();
            if(idx + 1 >= tokens.size()) throw std::runtime_error("missing redirect target after " + tok);
            current_pipe.output_redirect = tokens[idx + 1];
            current_pipe.redirect_append = (tok == ">>");
            ++idx;
            continue;
        }
        if(current_cmd.name.empty()){
            current_cmd.name = tok;
        } else {
            current_cmd.args.push_back(tok);
        }
    }

    if(!current_cmd.name.empty()) flush_command();
    if(!current_pipe.commands.empty()){
        chain.push_back(CommandChainEntry{next_logic, current_pipe});
        next_logic.clear();
    }
    if(!next_logic.empty()) throw std::runtime_error("dangling logical operator");
    return chain;
}

std::vector<std::string> get_all_commands(){
    return {
        "cd", "ls", "tree", "mkdir", "touch", "cat", "grep", "rg", "count",
        "history", "true", "false", "tail", "head", "uniq", "random", "echo",
        "rm", "mv", "link", "export", "parse", "eval", "ai", "ai.brief",
        "discuss", "ai.discuss", "discuss.session", "tools", "overlay.list",
        "overlay.use", "overlay.policy", "overlay.mount", "overlay.save",
        "overlay.unmount", "mount", "mount.lib", "mount.remote", "mount.list",
        "mount.allow", "mount.disallow", "unmount", "tag.add", "tag.remove",
        "tag.list", "tag.clear", "tag.has", "logic.init", "logic.infer",
        "logic.check", "logic.explain", "logic.addrule", "logic.listrules",
        "logic.assert", "logic.sat", "tag.mine.start", "tag.mine.feedback",
        "tag.mine.status", "plan.create", "plan.goto",
        "plan.forward", "plan.backward", "plan.context.add", "plan.context.remove",
        "plan.context.clear", "plan.context.list", "plan.status", "plan.discuss",
        "plan.answer", "plan.hypothesis", "plan.jobs.add", "plan.jobs.complete",
        "plan.verify", "plan.tags.infer", "plan.tags.check", "plan.validate",
        "plan.save", "solution.save", "context.build", "context.build.adv",
        "context.build.advanced", "context.filter.tag", "context.filter.path",
        "tree.adv", "tree.advanced", "test.planner", "test.hypothesis",
        "hypothesis.test", "hypothesis.query", "hypothesis.errorhandling",
        "hypothesis.duplicates", "hypothesis.logging", "hypothesis.pattern",
        "cpp.tu", "cpp.include", "cpp.func", "cpp.param", "cpp.print",
        "cpp.returni", "cpp.return", "cpp.expr", "cpp.vardecl", "cpp.stmt",
        "cpp.rangefor", "cpp.dump", "make", "sample.run", "help", "quit", "exit",
        "upp.load", "upp.create", "upp.list", "upp.scan", "upp.load.host", "upp.gui"
    };
}

std::vector<std::string> get_path_completions(Vfs& vfs, const std::string& partial, const std::string& cwd_path){
    std::vector<std::string> results;

    // Determine the directory to search and the prefix to match
    std::string search_dir = cwd_path;
    std::string prefix;

    if(partial.empty()){
        prefix = "";
    } else if(partial.back() == '/'){
        // Complete from the specified directory
        search_dir = normalize_path(cwd_path, partial);
        prefix = "";
    } else {
        // Split into directory and filename parts
        size_t last_slash = partial.rfind('/');
        if(last_slash != std::string::npos){
            std::string dir_part = partial.substr(0, last_slash + 1);
            prefix = partial.substr(last_slash + 1);
            search_dir = normalize_path(cwd_path, dir_part);
        } else {
            prefix = partial;
        }
    }

    // Look up the directory in VFS
    try {
        auto hits = vfs.resolveMulti(search_dir);
        if(hits.empty()) return results;

        // Check if it's a directory
        bool anyDir = false;
        std::vector<size_t> listingOverlays;
        for(const auto& hit : hits){
            if(hit.node->isDir()){
                anyDir = true;
                listingOverlays.push_back(hit.overlay_id);
            }
        }

        if(!anyDir) return results;

        // Get directory listing
        auto listing = vfs.listDir(search_dir, listingOverlays);
        for(const auto& [name, entry] : listing){
            if(name.empty() || name[0] == '.') continue; // skip hidden
            if(prefix.empty() || name.find(prefix) == 0){
                std::string completion = name;
                // Check if entry is a directory
                if(entry.types.count('d') > 0){
                    completion += '/';
                }
                results.push_back(completion);
            }
        }
    } catch(...) {
        // Path not found or error - return empty
        return results;
    }

    std::sort(results.begin(), results.end());
    return results;
}


// Perform tab completion on the current buffer
std::string complete_input(Vfs& vfs, const std::string& buffer, size_t cursor, const std::string& cwd_path, bool& show_list){
    show_list = false;

    // Only complete at cursor position (end of word)
    if(cursor != buffer.size()) return buffer;

    // Parse the input to determine context
    auto trimmed = trim_copy(buffer);
    if(trimmed.empty()) return buffer;

    // Simple tokenization
    std::vector<std::string> tokens;
    std::string current;
    bool in_quote = false;
    for(char ch : buffer){
        if(ch == '"' || ch == '\''){
            in_quote = !in_quote;
            current += ch;
        } else if(std::isspace(static_cast<unsigned char>(ch)) && !in_quote){
            if(!current.empty()){
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += ch;
        }
    }
    if(!current.empty()) tokens.push_back(current);

    if(tokens.empty()) return buffer;

    std::vector<std::string> candidates;
    std::string prefix_to_complete;
    bool completing_command = (tokens.size() == 1 && !buffer.empty() && !std::isspace(static_cast<unsigned char>(buffer.back())));

    if(completing_command){
        // Complete command names
        prefix_to_complete = tokens[0];
        auto all_cmds = get_all_commands();
        for(const auto& cmd : all_cmds){
            if(cmd.find(prefix_to_complete) == 0){
                candidates.push_back(cmd);
            }
        }
    } else {
        // Complete file paths
        prefix_to_complete = tokens.back();
        candidates = get_path_completions(vfs, prefix_to_complete, cwd_path);
    }

    if(candidates.empty()){
        return buffer; // No completions
    }

    if(candidates.size() == 1){
        // Single match - complete it
        std::string completion = candidates[0];
        std::string result = buffer.substr(0, buffer.size() - prefix_to_complete.size()) + completion;
        if(completing_command) result += ' '; // Add space after command
        return result;
    }

    // Multiple matches - find common prefix
    std::string common = candidates[0];
    for(size_t i = 1; i < candidates.size(); ++i){
        size_t j = 0;
        while(j < common.size() && j < candidates[i].size() && common[j] == candidates[i][j]){
            ++j;
        }
        common = common.substr(0, j);
    }

    if(common.size() > prefix_to_complete.size()){
        // Complete to common prefix
        return buffer.substr(0, buffer.size() - prefix_to_complete.size()) + common;
    }

    // Show list of candidates
    show_list = true;
    std::cout << '\n';
    size_t col = 0;
    const size_t max_width = 80;
    for(const auto& candidate : candidates){
        if(col + candidate.size() + 2 > max_width && col > 0){
            std::cout << '\n';
            col = 0;
        }
        std::cout << candidate << "  ";
        col += candidate.size() + 2;
    }
    std::cout << '\n';

    return buffer;
}

void save_history(const std::vector<std::string>& history){
    auto path_opt = history_file_path();
    if(!path_opt) return;
    const auto& path = *path_opt;
    std::error_code ec;
    auto parent = path.parent_path();
    if(!parent.empty()) std::filesystem::create_directories(parent, ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if(!out){
        TRACE_MSG("history write failed: ", path.string());
        return;
    }
    for(const auto& entry : history){
        out << entry << '\n';
    }
}

