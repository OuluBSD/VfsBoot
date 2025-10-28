#include "make.h"

#include "VfsShell.h"

void MakeFile::parse(const std::string& content) {
    graph_.rules.clear();
    variables_.clear();
    phony_targets_.clear();
    rule_order_.clear();

    std::istringstream stream(content);
    std::string line;
    std::string current_target;

    while(std::getline(stream, line)) {
        if(line.empty() || line[0] == '#') {
            continue;
        }
        parseLine(line, current_target);
    }

    finalizePhonyMarkers();
}

void MakeFile::parseLine(const std::string& line, std::string& current_target) {
    if(!line.empty() && line[0] == '\t') {
        if(current_target.empty()) {
            throw std::runtime_error("make: command without target");
        }
        auto it = graph_.rules.find(current_target);
        if(it == graph_.rules.end()) {
            throw std::runtime_error("make: command without defined target: " + current_target);
        }
        BuildCommand cmd;
        cmd.type = BuildCommand::Type::Shell;
        cmd.text = line.substr(1);
        it->second.commands.push_back(cmd);
        return;
    }

    size_t eq_pos = line.find('=');
    if(eq_pos != std::string::npos && line.find(':') > eq_pos) {
        std::string var_name = line.substr(0, eq_pos);
        std::string var_value = eq_pos + 1 < line.size() ? line.substr(eq_pos + 1) : "";

        var_name.erase(0, var_name.find_first_not_of(" \t"));
        var_name.erase(var_name.find_last_not_of(" \t") + 1);
        var_value.erase(0, var_value.find_first_not_of(" \t"));
        var_value.erase(var_value.find_last_not_of(" \t") + 1);

        variables_[var_name] = var_value;
        current_target.clear();
        return;
    }

    size_t colon_pos = line.find(':');
    if(colon_pos != std::string::npos) {
        std::string target = line.substr(0, colon_pos);
        std::string deps_str = colon_pos + 1 < line.size() ? line.substr(colon_pos + 1) : "";

        target.erase(0, target.find_first_not_of(" \t"));
        target.erase(target.find_last_not_of(" \t") + 1);

        if(target == ".PHONY") {
            std::istringstream deps_iss(deps_str);
            std::string phony;
            while(deps_iss >> phony) {
                phony_targets_.insert(phony);
                auto it = graph_.rules.find(phony);
                if(it != graph_.rules.end()) {
                    it->second.always_run = true;
                }
            }
            current_target.clear();
            return;
        }

        bool new_rule = !graph_.rules.count(target);
        BuildRule rule;
        rule.name = target;
        rule.always_run = phony_targets_.count(target) > 0;

        std::istringstream deps_iss(deps_str);
        std::string dep;
        while(deps_iss >> dep) {
            rule.dependencies.push_back(dep);
        }

        graph_.rules[target] = rule;
        if(new_rule) {
            rule_order_.push_back(target);
        }

        current_target = target;
        return;
    }

    current_target.clear();
}

std::string MakeFile::expandVariables(const std::string& text) const {
    std::string result = text;
    size_t pos = 0;
    while((pos = result.find("$(", pos)) != std::string::npos) {
        size_t end = result.find(')', pos + 2);
        if(end == std::string::npos) break;
        std::string var_name = result.substr(pos + 2, end - pos - 2);
        auto it = variables_.find(var_name);
        std::string value = (it != variables_.end()) ? it->second : "";
        result.replace(pos, end - pos + 1, value);
        pos += value.length();
    }

    pos = 0;
    while((pos = result.find("${", pos)) != std::string::npos) {
        size_t end = result.find('}', pos + 2);
        if(end == std::string::npos) break;
        std::string var_name = result.substr(pos + 2, end - pos - 2);
        auto it = variables_.find(var_name);
        std::string value = (it != variables_.end()) ? it->second : "";
        result.replace(pos, end - pos + 1, value);
        pos += value.length();
    }

    return result;
}

std::string MakeFile::expandAutomaticVars(const std::string& text, const BuildRule& rule) const {
    std::string result = text;

    size_t pos = 0;
    while((pos = result.find("$@", pos)) != std::string::npos) {
        result.replace(pos, 2, rule.name);
        pos += rule.name.length();
    }

    if(!rule.dependencies.empty()) {
        pos = 0;
        while((pos = result.find("$<", pos)) != std::string::npos) {
            result.replace(pos, 2, rule.dependencies.front());
            pos += rule.dependencies.front().length();
        }
    }

    if(!rule.dependencies.empty()) {
        std::ostringstream deps;
        for(size_t i = 0; i < rule.dependencies.size(); ++i) {
            if(i) deps << " ";
            deps << rule.dependencies[i];
        }
        std::string deps_str = deps.str();
        pos = 0;
        while((pos = result.find("$^", pos)) != std::string::npos) {
            result.replace(pos, 2, deps_str);
            pos += deps_str.length();
        }
    }

    return result;
}

void MakeFile::finalizePhonyMarkers() {
    for(const auto& target : phony_targets_) {
        auto it = graph_.rules.find(target);
        if(it != graph_.rules.end()) {
            it->second.always_run = true;
        }
    }
}

BuildResult MakeFile::build(const std::string& target, Vfs& vfs, bool verbose) {
    finalizePhonyMarkers();

    BuildOptions options;
    options.verbose = verbose;
    options.executor = [this](const BuildRule& rule, BuildResult& result, bool v) {
        BuildRule expanded = rule;
        for(auto& cmd : expanded.commands) {
            if(cmd.type != BuildCommand::Type::Shell) continue;
            std::string text = expandVariables(cmd.text);
            text = expandAutomaticVars(text, rule);
            cmd.text = text;
        }
        return BuildGraph::runShellCommands(expanded, result, v);
    };

    return graph_.build(target, vfs, options);
}

