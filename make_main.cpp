// Standalone make utility for VfsBoot bootstrap
// Minimal subset implementation extracted from codex

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Makefile rule representation
struct MakeRule {
    std::string target;
    std::vector<std::string> dependencies;
    std::vector<std::string> commands;
    bool is_phony;

    MakeRule() : is_phony(false) {}
    MakeRule(std::string t) : target(std::move(t)), is_phony(false) {}
};

// Makefile parser and executor
struct MakeFile {
    std::map<std::string, std::string> variables;
    std::map<std::string, MakeRule> rules;
    std::set<std::string> phony_targets;

    void parse(const std::string& content);
    std::string expandVariables(const std::string& text) const;
    std::string expandAutomaticVars(const std::string& text, const MakeRule& rule) const;
    bool needsRebuild(const std::string& target) const;
    std::optional<uint64_t> getModTime(const std::string& path) const;

    struct BuildResult {
        bool success;
        std::string output;
        std::vector<std::string> targets_built;
        std::vector<std::string> errors;
    };

    BuildResult build(const std::string& target, bool verbose = false);

private:
    bool buildTarget(const std::string& target,
                    std::set<std::string>& building,
                    std::set<std::string>& built,
                    BuildResult& result, bool verbose);
    bool executeCommands(const MakeRule& rule, BuildResult& result, bool verbose);
    void parseLine(const std::string& line, std::string& current_target);
};

void MakeFile::parse(const std::string& content) {
    std::istringstream iss(content);
    std::string line, current_target;

    while(std::getline(iss, line)) {
        if(line.empty() || line[0] == '#') continue;
        parseLine(line, current_target);
    }
}

void MakeFile::parseLine(const std::string& line, std::string& current_target) {
    // Tab-indented command
    if(!line.empty() && line[0] == '\t') {
        if(current_target.empty()) {
            throw std::runtime_error("make: command without target");
        }
        std::string cmd = line.substr(1);
        rules[current_target].commands.push_back(cmd);
        return;
    }

    // Variable assignment (handle =, :=, ?=)
    size_t eq_pos = line.find('=');
    if(eq_pos != std::string::npos && eq_pos > 0 && line.find(':') > eq_pos) {
        // Check for := or ?=
        size_t assign_start = eq_pos;
        if(line[eq_pos - 1] == ':' || line[eq_pos - 1] == '?') {
            assign_start = eq_pos - 1;
        }

        std::string var_name = line.substr(0, assign_start);
        std::string var_value = eq_pos + 1 < line.size() ? line.substr(eq_pos + 1) : "";

        // Trim whitespace
        var_name.erase(0, var_name.find_first_not_of(" \t"));
        var_name.erase(var_name.find_last_not_of(" \t") + 1);
        var_value.erase(0, var_value.find_first_not_of(" \t"));
        var_value.erase(var_value.find_last_not_of(" \t") + 1);

        // ?= only assigns if variable doesn't exist (check environment too)
        if(line[eq_pos - 1] == '?') {
            if(variables.find(var_name) != variables.end()) {
                current_target.clear();
                return;
            }
            // Check environment variable
            const char* env_val = std::getenv(var_name.c_str());
            if(env_val) {
                variables[var_name] = env_val;
                current_target.clear();
                return;
            }
        }

        // Expand variables in value immediately for := assignment
        if(assign_start < eq_pos && line[eq_pos - 1] == ':') {
            var_value = expandVariables(var_value);
        }

        variables[var_name] = var_value;
        current_target.clear();
        return;
    }

    // Rule: target: dependencies
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
                phony_targets.insert(phony);
            }
            current_target.clear();
            return;
        }

        MakeRule rule(target);
        rule.is_phony = (phony_targets.count(target) > 0);

        // Expand variables in dependencies
        std::string expanded_deps = expandVariables(deps_str);

        std::istringstream deps_iss(expanded_deps);
        std::string dep;
        while(deps_iss >> dep) {
            rule.dependencies.push_back(dep);
        }

        rules[target] = rule;
        current_target = target;
        return;
    }

    current_target.clear();
}

std::string MakeFile::expandVariables(const std::string& text) const {
    std::string result = text;

    // Expand $(shell ...) and $(VAR)
    {
        size_t pos = 0;
        while((pos = result.find("$(", pos)) != std::string::npos) {
            size_t end = result.find(')', pos + 2);
            if(end == std::string::npos) break;

            std::string content = result.substr(pos + 2, end - pos - 2);
            std::string value;

            // Check if it's a shell command
            if(content.substr(0, 6) == "shell ") {
                std::string shell_cmd = content.substr(6);
                // Execute shell command and capture output
                FILE* pipe = popen(shell_cmd.c_str(), "r");
                if(pipe) {
                    char buffer[256];
                    while(fgets(buffer, sizeof(buffer), pipe)) {
                        value += buffer;
                    }
                    pclose(pipe);
                    // Trim trailing newline
                    if(!value.empty() && value.back() == '\n') {
                        value.pop_back();
                    }
                }
            } else {
                // Regular variable
                auto it = variables.find(content);
                value = (it != variables.end()) ? it->second : "";
            }

            result.replace(pos, end - pos + 1, value);
            pos += value.length();
        }
    }

    // Expand ${VAR}
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

    // $^ = all prerequisites
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

std::optional<uint64_t> MakeFile::getModTime(const std::string& path) const {
    struct stat st;
    if(stat(path.c_str(), &st) == 0) {
        return static_cast<uint64_t>(st.st_mtime);
    }
    return std::nullopt;
}

bool MakeFile::needsRebuild(const std::string& target) const {
    auto rule_it = rules.find(target);
    if(rule_it == rules.end()) return false;

    const MakeRule& rule = rule_it->second;

    if(rule.is_phony) return true;

    auto target_mtime = getModTime(target);
    if(!target_mtime) return true;

    for(const std::string& dep : rule.dependencies) {
        auto dep_mtime = getModTime(dep);
        if(!dep_mtime) return true;
        if(*dep_mtime > *target_mtime) return true;
    }

    return false;
}

bool MakeFile::executeCommands(const MakeRule& rule, BuildResult& result, bool verbose) {
    for(const std::string& cmd : rule.commands) {
        std::string expanded = expandVariables(cmd);
        expanded = expandAutomaticVars(expanded, rule);

        if(verbose) {
            result.output += expanded + "\n";
        }

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

bool MakeFile::buildTarget(const std::string& target,
                            std::set<std::string>& building,
                            std::set<std::string>& built,
                            BuildResult& result, bool verbose) {
    if(building.count(target)) {
        result.errors.push_back("Circular dependency detected: " + target);
        return false;
    }

    if(built.count(target)) {
        return true;
    }

    auto rule_it = rules.find(target);
    if(rule_it == rules.end()) {
        if(getModTime(target)) {
            built.insert(target);
            return true;
        }
        result.errors.push_back("No rule to make target: " + target);
        return false;
    }

    const MakeRule& rule = rule_it->second;

    building.insert(target);

    for(const std::string& dep : rule.dependencies) {
        if(!buildTarget(dep, building, built, result, verbose)) {
            building.erase(target);
            return false;
        }
    }

    if(needsRebuild(target)) {
        if(verbose) {
            result.output += "Building target: " + target + "\n";
        }

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

    building.erase(target);
    built.insert(target);

    return true;
}

MakeFile::BuildResult MakeFile::build(const std::string& target, bool verbose) {
    BuildResult result;
    result.success = false;

    std::set<std::string> building;
    std::set<std::string> built;

    result.success = buildTarget(target, building, built, result, verbose);

    return result;
}

// Main program
int main(int argc, char* argv[]) {
    std::string makefile_path = "Makefile";
    std::string target = "all";
    bool verbose = false;

    // Parse arguments
    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "-f" && i + 1 < argc) {
            makefile_path = argv[++i];
        } else if(arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if(arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [target] [-f makefile] [-v|--verbose]\n";
            std::cout << "\nMinimal GNU make subset for VfsBoot bootstrap\n";
            std::cout << "\nOptions:\n";
            std::cout << "  -f FILE        Read FILE as makefile (default: Makefile)\n";
            std::cout << "  -v, --verbose  Verbose output\n";
            std::cout << "  -h, --help     Show this help\n";
            return 0;
        } else {
            target = arg;
        }
    }

    // Read Makefile
    std::ifstream file(makefile_path);
    if(!file) {
        std::cerr << "make: Cannot read " << makefile_path << "\n";
        return 1;
    }

    std::string makefile_content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
    file.close();

    // Parse Makefile
    MakeFile makefile;
    try {
        makefile.parse(makefile_content);
    } catch(const std::exception& e) {
        std::cerr << "make: Parse error: " << e.what() << "\n";
        return 1;
    }

    // If target is "all" and not defined, use first rule
    if(target == "all" && makefile.rules.find("all") == makefile.rules.end()) {
        if(!makefile.rules.empty()) {
            target = makefile.rules.begin()->first;
        }
    }

    // Build target
    auto build_result = makefile.build(target, verbose);

    // Display output
    if(!build_result.output.empty()) {
        std::cout << build_result.output;
    }

    // Display results
    if(build_result.success) {
        if(build_result.targets_built.empty()) {
            std::cout << "make: '" << target << "' is up to date.\n";
        } else if(verbose) {
            std::cout << "make: Successfully built " << build_result.targets_built.size() << " target(s)\n";
        }
        return 0;
    } else {
        std::cerr << "make: *** ";
        if(!build_result.errors.empty()) {
            std::cerr << build_result.errors[0];
        } else {
            std::cerr << "Build failed";
        }
        std::cerr << "\n";
        return 1;
    }
}
