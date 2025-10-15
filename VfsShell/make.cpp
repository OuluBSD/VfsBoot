#include "VfsShell.h"

// ============================================================================
// Minimal GNU Make Implementation
// ============================================================================

void MakeFile::parse(const std::string& content) {
    std::istringstream iss(content);
    std::string line, current_target;

    while(std::getline(iss, line)) {
        // Skip empty lines and comments
        if(line.empty() || line[0] == '#') continue;

        parseLine(line, current_target);
    }
}

void MakeFile::parseLine(const std::string& line, std::string& current_target) {
    // Check if it's a continuation of command (starts with tab)
    if(!line.empty() && line[0] == '\t') {
        if(current_target.empty()) {
            throw std::runtime_error("make: command without target");
        }
        // Remove leading tab and add to current rule's commands
        std::string cmd = line.substr(1);
        rules[current_target].commands.push_back(cmd);
        return;
    }

    // Variable assignment: VAR = value or VAR := value
    size_t eq_pos = line.find('=');
    if(eq_pos != std::string::npos && line.find(':') > eq_pos) {
        std::string var_name = line.substr(0, eq_pos);
        std::string var_value = eq_pos + 1 < line.size() ? line.substr(eq_pos + 1) : "";

        // Trim whitespace
        var_name.erase(0, var_name.find_first_not_of(" \t"));
        var_name.erase(var_name.find_last_not_of(" \t") + 1);
        var_value.erase(0, var_value.find_first_not_of(" \t"));
        var_value.erase(var_value.find_last_not_of(" \t") + 1);

        variables[var_name] = var_value;
        current_target.clear();
        return;
    }

    // Rule: target: dependencies
    size_t colon_pos = line.find(':');
    if(colon_pos != std::string::npos) {
        std::string target = line.substr(0, colon_pos);
        std::string deps_str = colon_pos + 1 < line.size() ? line.substr(colon_pos + 1) : "";

        // Trim whitespace
        target.erase(0, target.find_first_not_of(" \t"));
        target.erase(target.find_last_not_of(" \t") + 1);

        // Handle .PHONY special target
        if(target == ".PHONY") {
            // Parse phony targets from dependencies
            std::istringstream deps_iss(deps_str);
            std::string phony;
            while(deps_iss >> phony) {
                phony_targets.insert(phony);
            }
            current_target.clear();
            return;
        }

        // Create new rule
        MakeRule rule(target);
        rule.is_phony = (phony_targets.count(target) > 0);

        // Parse dependencies
        std::istringstream deps_iss(deps_str);
        std::string dep;
        while(deps_iss >> dep) {
            rule.dependencies.push_back(dep);
        }

        rules[target] = rule;
        current_target = target;
        return;
    }

    // Otherwise, clear current target
    current_target.clear();
}

std::string MakeFile::expandVariables(const std::string& text) const {
    std::string result = text;

    // Expand $(VAR) style
    {
        size_t pos = 0;
        while((pos = result.find("$(", pos)) != std::string::npos) {
            size_t end = result.find(')', pos + 2);
            if(end == std::string::npos) break;

            std::string var_name = result.substr(pos + 2, end - pos - 2);
            auto it = variables.find(var_name);
            std::string value = (it != variables.end()) ? it->second : "";

            result.replace(pos, end - pos + 1, value);
            pos += value.length();
        }
    }

    // Expand ${VAR} style
    {
        size_t pos = 0;
        while((pos = result.find("${", pos)) != std::string::npos) {
            size_t end = result.find('}', pos + 2);
            if(end == std::string::npos) break;

            std::string var_name = result.substr(pos + 2, end - pos - 2);
            auto it = variables.find(var_name);
            std::string value = (it != variables.end()) ? it->second : "";

            result.replace(pos, end - pos + 1, value);
            pos += value.length();
        }
    }

    return result;
}

std::string MakeFile::expandAutomaticVars(const std::string& text, const MakeRule& rule) const {
    std::string result = text;

    // $@ = target name
    {
        size_t pos = 0;
        while((pos = result.find("$@", pos)) != std::string::npos) {
            result.replace(pos, 2, rule.target);
            pos += rule.target.length();
        }
    }

    // $< = first prerequisite
    if(!rule.dependencies.empty()) {
        size_t pos = 0;
        while((pos = result.find("$<", pos)) != std::string::npos) {
            result.replace(pos, 2, rule.dependencies[0]);
            pos += rule.dependencies[0].length();
        }
    }

    // $^ = all prerequisites (space-separated)
    if(!rule.dependencies.empty()) {
        std::ostringstream all_deps;
        for(size_t i = 0; i < rule.dependencies.size(); ++i) {
            if(i > 0) all_deps << " ";
            all_deps << rule.dependencies[i];
        }

        size_t pos = 0;
        while((pos = result.find("$^", pos)) != std::string::npos) {
            std::string deps_str = all_deps.str();
            result.replace(pos, 2, deps_str);
            pos += deps_str.length();
        }
    }

    return result;
}

std::optional<uint64_t> MakeFile::getModTime(const std::string& path, Vfs& vfs) const {
    // Try VFS first
    try {
        vfs.resolve(path);
        // VFS doesn't track mtimes natively, so assume 0 for now
        // Real implementation would need mtime tracking in VfsNode
        return 0;
    } catch(...) {
        // Try filesystem
        try {
            struct stat st;
            if(stat(path.c_str(), &st) == 0) {
                return static_cast<uint64_t>(st.st_mtime);
            }
        } catch(...) {}
    }

    return std::nullopt;
}

bool MakeFile::needsRebuild(const std::string& target, Vfs& vfs) const {
    auto rule_it = rules.find(target);
    if(rule_it == rules.end()) return false;

    const MakeRule& rule = rule_it->second;

    // .PHONY targets always rebuild
    if(rule.is_phony) return true;

    // Get target modification time
    auto target_mtime = getModTime(target, vfs);

    // If target doesn't exist, needs rebuild
    if(!target_mtime) return true;

    // Check if any dependency is newer than target
    for(const std::string& dep : rule.dependencies) {
        auto dep_mtime = getModTime(dep, vfs);
        if(!dep_mtime) {
            // Dependency doesn't exist - need to build it first
            return true;
        }
        if(*dep_mtime > *target_mtime) {
            return true;
        }
    }

    return false;
}

bool MakeFile::executeCommands(const MakeRule& rule, BuildResult& result, bool verbose) {
    for(const std::string& cmd : rule.commands) {
        // Expand variables
        std::string expanded = expandVariables(cmd);
        expanded = expandAutomaticVars(expanded, rule);

        if(verbose) {
            result.output += expanded + "\n";
        }

        // Execute command
        FILE* pipe = popen(expanded.c_str(), "r");
        if(!pipe) {
            result.errors.push_back("Failed to execute: " + expanded);
            return false;
        }

        char buffer[256];
        while(fgets(buffer, sizeof(buffer), pipe)) {
            result.output += buffer;
        }

        int status = pclose(pipe);
        if(status != 0) {
            result.errors.push_back("Command failed (exit " + std::to_string(WEXITSTATUS(status)) + "): " + expanded);
            return false;
        }
    }

    return true;
}

bool MakeFile::buildTarget(const std::string& target, Vfs& vfs,
                            std::set<std::string>& building,
                            std::set<std::string>& built,
                            BuildResult& result, bool verbose) {
    // Check for cycles
    if(building.count(target)) {
        result.errors.push_back("Circular dependency detected: " + target);
        return false;
    }

    // Already built?
    if(built.count(target)) {
        return true;
    }

    // Find rule for this target
    auto rule_it = rules.find(target);
    if(rule_it == rules.end()) {
        // No rule - check if file exists
        if(getModTime(target, vfs)) {
            built.insert(target);
            return true;
        }
        result.errors.push_back("No rule to make target: " + target);
        return false;
    }

    const MakeRule& rule = rule_it->second;

    // Mark as building (cycle detection)
    building.insert(target);

    // Build all dependencies first
    for(const std::string& dep : rule.dependencies) {
        if(!buildTarget(dep, vfs, building, built, result, verbose)) {
            building.erase(target);
            return false;
        }
    }

    // Check if rebuild is needed
    if(needsRebuild(target, vfs)) {
        if(verbose) {
            result.output += "Building target: " + target + "\n";
        }

        // Execute commands
        if(!executeCommands(rule, result, verbose)) {
            building.erase(target);
            return false;
        }

        result.targets_built.push_back(target);
    } else {
        if(verbose) {
            result.output += "Target up-to-date: " + target + "\n";
        }
    }

    // Mark as built
    building.erase(target);
    built.insert(target);

    return true;
}

MakeFile::BuildResult MakeFile::build(const std::string& target, Vfs& vfs, bool verbose) {
    BuildResult result;
    result.success = false;

    std::set<std::string> building;
    std::set<std::string>built;

    result.success = buildTarget(target, vfs, building, built, result, verbose);

    return result;
}

