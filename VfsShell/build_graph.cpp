#include "build_graph.h"

#include "VfsShell.h"

#include <cstdio>
#include <sys/stat.h>
#include <sys/wait.h>

namespace {

std::string joinOutputs(const std::vector<std::string>& outputs) {
    if(outputs.empty()) return {};
    std::string result;
    for(size_t i = 0; i < outputs.size(); ++i) {
        if(i) result += ", ";
        result += outputs[i];
    }
    return result;
}

} // namespace

BuildResult BuildGraph::build(const std::string& target, Vfs& vfs, BuildOptions options) {
    BuildResult result;
    result.success = false;

    if(!options.executor) {
        options.executor = [](const BuildRule& rule, BuildResult& res, bool verbose) {
            return BuildGraph::runShellCommands(rule, res, verbose);
        };
    }

    std::set<std::string> visiting;
    std::set<std::string> built;

    if(rules.find(target) == rules.end()) {
        // Allow building files directly if they exist in VFS/host
        try {
            vfs.resolve(target);
            result.success = true;
            return result;
        } catch(...) {
            result.errors.push_back("No rule to build target: " + target);
            return result;
        }
    }

    if(buildNode(target, vfs, options, visiting, built, result)) {
        result.success = true;
    }

    return result;
}

bool BuildGraph::runShellCommands(const BuildRule& rule, BuildResult& result, bool verbose) {
    for(const auto& command : rule.commands) {
        if(command.type != BuildCommand::Type::Shell) {
            result.errors.push_back("Unsupported command type for rule: " + rule.name);
            return false;
        }

        if(verbose) {
            result.output += command.text + "\n";
        }

        FILE* pipe = popen(command.text.c_str(), "r");
        if(!pipe) {
            result.errors.push_back("Failed to execute: " + command.text);
            return false;
        }

        char buffer[256];
        while(fgets(buffer, sizeof(buffer), pipe)) {
            result.output += buffer;
        }

        int status = pclose(pipe);
        if(status != 0) {
            result.errors.push_back("Command failed (exit " +
                                    std::to_string(WEXITSTATUS(status)) +
                                    "): " + command.text);
            return false;
        }
    }

    return true;
}

bool BuildGraph::buildNode(const std::string& target,
                           Vfs& vfs,
                           BuildOptions& options,
                           std::set<std::string>& visiting,
                           std::set<std::string>& built,
                           BuildResult& result) {
    if(visiting.count(target)) {
        result.errors.push_back("Circular dependency detected: " + target);
        return false;
    }
    if(built.count(target)) {
        return true;
    }

    auto it = rules.find(target);
    if(it == rules.end()) {
        // Fallback to checking file presence
        try {
            vfs.resolve(target);
            built.insert(target);
            return true;
        } catch(...) {
            result.errors.push_back("No rule to build target: " + target);
            return false;
        }
    }

    const BuildRule& rule = it->second;
    visiting.insert(target);

    for(const auto& dep : rule.dependencies) {
        if(rules.find(dep) != rules.end()) {
            if(!buildNode(dep, vfs, options, visiting, built, result)) {
                visiting.erase(target);
                return false;
            }
        } else {
            // Ensure dependency exists when treated as file
            try {
                vfs.resolve(dep);
            } catch(...) {
                auto dep_time = getModTime(dep);
                if(!dep_time) {
                    // Defer to need-to-build logic (likely generated file)
                    if(options.verbose) {
                        result.output += "Dependency missing (will rely on rule): " + dep + "\n";
                    }
                }
            }
        }
    }

    if(needsRebuild(rule, vfs, options)) {
        if(options.verbose) {
            std::string outputs = joinOutputs(rule.outputs);
            if(outputs.empty()) outputs = rule.name;
            result.output += "Building " + rule.name + " -> " + outputs + "\n";
        }
        if(!options.executor(rule, result, options.verbose)) {
            visiting.erase(target);
            return false;
        }
        result.targets_built.push_back(rule.name);
    } else if(options.verbose) {
        result.output += "Target up-to-date: " + rule.name + "\n";
    }

    visiting.erase(target);
    built.insert(target);
    return true;
}

bool BuildGraph::needsRebuild(const BuildRule& rule, Vfs& vfs, BuildOptions& options) {
    if(rule.always_run) {
        return true;
    }

    auto output_time = options.output_time_override
        ? options.output_time_override(rule, vfs)
        : determineOutputTime(rule, vfs);

    if(!output_time) {
        return true;
    }

    for(const auto& dep : rule.dependencies) {
        if(auto rule_it = rules.find(dep); rule_it != rules.end()) {
            auto dep_time = options.output_time_override
                ? options.output_time_override(rule_it->second, vfs)
                : determineOutputTime(rule_it->second, vfs);
            if(!dep_time || *dep_time > *output_time) {
                return true;
            }
            continue;
        }

        auto dep_time = getModTime(dep);
        if(!dep_time) {
            return true;
        }
        if(*dep_time > *output_time) {
            return true;
        }
    }

    return false;
}

std::optional<uint64_t> BuildGraph::determineOutputTime(const BuildRule& rule, Vfs& vfs) {
    std::vector<std::string> outputs = rule.outputs;
    if(outputs.empty()) {
        outputs.push_back(rule.name);
    }

    std::optional<uint64_t> min_time;
    for(const auto& path : outputs) {
        try {
            vfs.resolve(path);
            // VFS nodes do not track mtime; treat as zero
            if(!min_time || *min_time > 0) {
                min_time = 0;
            }
            continue;
        } catch(...) {
            // Fall through to filesystem check
        }

        auto host_time = getModTime(path);
        if(!host_time) {
            return std::nullopt;
        }

        if(!min_time || *host_time < *min_time) {
            min_time = host_time;
        }
    }

    return min_time;
}

std::optional<uint64_t> BuildGraph::getModTime(const std::string& path) {
    struct stat st{};
    if(stat(path.c_str(), &st) == 0) {
        return static_cast<uint64_t>(st.st_mtime);
    }
    return std::nullopt;
}

