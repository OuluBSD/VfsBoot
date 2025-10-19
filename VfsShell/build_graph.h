#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

class Vfs;

// Generic build graph primitives shared by make/upp builders
struct BuildCommand {
    enum class Type {
        Shell,
        AiPrompt
    };

    Type type = Type::Shell;
    std::string text;
    std::map<std::string, std::string> metadata;
};

struct BuildRule {
    std::string name;
    std::vector<std::string> dependencies;
    std::vector<BuildCommand> commands;
    bool always_run = false;
    std::vector<std::string> outputs;
};

struct BuildResult {
    bool success = false;
    std::string output;
    std::vector<std::string> targets_built;
    std::vector<std::string> errors;
};

struct BuildOptions {
    bool verbose = false;
    std::function<bool(const BuildRule&, BuildResult&, bool verbose)> executor;
    std::function<std::optional<uint64_t>(const BuildRule&, Vfs&)> output_time_override;
};

class BuildGraph {
public:
    std::map<std::string, BuildRule> rules;

    BuildResult build(const std::string& target, Vfs& vfs, BuildOptions options = {});

    static bool runShellCommands(const BuildRule& rule, BuildResult& result, bool verbose);

private:
    bool buildNode(const std::string& target,
                   Vfs& vfs,
                   BuildOptions& options,
                   std::set<std::string>& visiting,
                   std::set<std::string>& built,
                   BuildResult& result);

    bool needsRebuild(const BuildRule& rule, Vfs& vfs, BuildOptions& options);
    std::optional<uint64_t> determineOutputTime(const BuildRule& rule, Vfs& vfs);
    static std::optional<uint64_t> getModTime(const std::string& path);
};
