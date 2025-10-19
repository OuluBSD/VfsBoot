#include "upp_workspace_build.h"

#include "VfsShell.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unordered_set>

extern UppBuilderRegistry g_upp_builder_registry;

namespace {

std::string shell_quote(const std::string& value) {
    if(value.empty()) return "''";
    std::string quoted = "'";
    for(char ch : value) {
        if(ch == '\'') {
            quoted += "'\"'\"'";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

std::vector<std::string> split_env_paths(const std::string& value) {
    std::vector<std::string> paths;
    std::stringstream ss(value);
    std::string item;
    while(std::getline(ss, item, ':')) {
        if(!item.empty()) paths.push_back(item);
    }
    return paths;
}

std::string join_with(const std::vector<std::string>& items, char delimiter) {
    if(items.empty()) return {};
    std::ostringstream oss;
    for(size_t i = 0; i < items.size(); ++i) {
        if(i) oss << delimiter;
        oss << items[i];
    }
    return oss.str();
}

std::string package_target(const std::string& name) {
    return "pkg:" + name;
}

std::string prefer_host_path(Vfs& vfs, const std::string& path) {
    if(path.empty()) return path;
    if(auto mapped = vfs.mapToHostPath(path)) {
        return *mapped;
    }
    return path;
}

void collect_packages(const std::shared_ptr<UppWorkspace>& workspace,
                      const std::string& pkg_name,
                      std::unordered_set<std::string>& visiting,
                      std::unordered_set<std::string>& visited,
                      std::vector<std::string>& order) {
    if(visited.count(pkg_name)) return;
    if(visiting.count(pkg_name)) {
        throw std::runtime_error("Circular package dependency detected around '" + pkg_name + "'");
    }

    visiting.insert(pkg_name);
    auto pkg = workspace->get_package(pkg_name);
    if(pkg) {
        for(const auto& dep : pkg->dependencies) {
            if(workspace->get_package(dep)) {
                collect_packages(workspace, dep, visiting, visited, order);
            }
        }
    }
    visiting.erase(pkg_name);

    visited.insert(pkg_name);
    order.push_back(pkg_name);
}

std::vector<std::string> build_asmlist(const UppWorkspace& workspace,
                                       const UppPackage& pkg,
                                       const WorkspaceBuildOptions& options,
                                       Vfs& vfs,
                                       const UppBuildMethod* builder) {
    std::unordered_set<std::string> dirs;
    auto capture = [&](const std::string& raw) {
        if(raw.empty()) return;
        auto normalized = prefer_host_path(vfs, raw);
        dirs.insert(std::filesystem::path(normalized).lexically_normal().string());
    };

    if(!workspace.base_dir.empty()) capture(workspace.base_dir);

    if(!workspace.assembly_path.empty()) {
        auto parent = std::filesystem::path(workspace.assembly_path).parent_path();
        if(!parent.empty()) capture(parent.string());
    }

    if(!pkg.path.empty()) {
        std::filesystem::path pkg_path(pkg.path);
        if(pkg_path.is_relative() && !workspace.base_dir.empty()) {
            pkg_path = std::filesystem::path(workspace.base_dir) / pkg_path;
        }
        auto parent = pkg_path.parent_path();
        if(!parent.empty()) capture(parent.lexically_normal().string());
    }

    for(const auto& inc : options.extra_includes) {
        capture(inc);
    }

    if(builder) {
        for(const auto& inc : builder->splitList("INCLUDES", ';')) {
            capture(inc);
        }
    }

    if(const char* upp_env = std::getenv("UPP")) {
        for(const auto& inc : split_env_paths(upp_env)) {
            capture(inc);
        }
    }

    std::vector<std::string> result(dirs.begin(), dirs.end());
    std::sort(result.begin(), result.end());
    return result;
}

std::string umk_flags(const WorkspaceBuildOptions& options, bool verbose) {
    std::string flags;
    if(options.build_type == "release") {
        flags = "-r";
    } else {
        flags = "-d";
    }
    if(verbose || options.verbose) flags += "v";
    return flags;
}

std::string default_output_path(const UppWorkspace& workspace,
                                const UppPackage& pkg,
                                const WorkspaceBuildOptions& options,
                                Vfs& vfs) {
    if(!options.output_dir.empty()) {
        std::filesystem::path base(options.output_dir);
        if(base.is_relative()) {
            if(!workspace.base_dir.empty()) {
                base = std::filesystem::path(workspace.base_dir) / base;
            }
        }
        base /= pkg.name;
        return prefer_host_path(vfs, base.lexically_normal().string());
    }

    if(!workspace.base_dir.empty()) {
        std::filesystem::path out_dir = std::filesystem::path(workspace.base_dir) / "out";
        out_dir /= pkg.name;
        return prefer_host_path(vfs, out_dir.lexically_normal().string());
    }

    return {};
}

std::string render_command_template(const std::string& tpl,
                                    const std::map<std::string, std::string>& vars) {
    std::string result = tpl;
    for(const auto& [key, value] : vars) {
        std::string marker = "{" + key + "}";
        size_t pos = 0;
        while((pos = result.find(marker, pos)) != std::string::npos) {
            result.replace(pos, marker.length(), value);
            pos += value.length();
        }
    }
    return result;
}

std::string make_command_for_package(const UppWorkspace& workspace,
                                     const UppPackage& pkg,
                                     const WorkspaceBuildOptions& options,
                                     Vfs& vfs,
                                     const UppBuildMethod* builder) {
    auto assembly_dirs = build_asmlist(workspace, pkg, options, vfs, builder);
    std::string assembly_arg = assembly_dirs.empty() ? "." : join_with(assembly_dirs, ',');
    std::string flags = umk_flags(options, options.verbose);
    std::string output_path = default_output_path(workspace, pkg, options, vfs);

    std::filesystem::path pkg_path_fs;
    if(!pkg.path.empty()) {
        pkg_path_fs = std::filesystem::path(pkg.path);
        if(pkg_path_fs.is_relative() && !workspace.base_dir.empty()) {
            pkg_path_fs = std::filesystem::path(workspace.base_dir) / pkg_path_fs;
        }
    }

    std::string package_path;
    if(!pkg.path.empty()) {
        std::filesystem::path to_render = pkg_path_fs.empty()
            ? std::filesystem::path(pkg.path)
            : pkg_path_fs;
        package_path = prefer_host_path(vfs, to_render.lexically_normal().string());
    }

    std::string base_dir = workspace.base_dir.empty()
        ? ""
        : prefer_host_path(vfs, workspace.base_dir);

    std::map<std::string, std::string> vars = {
        {"assembly", shell_quote(assembly_arg)},
        {"package", shell_quote(pkg.name)},
        {"package_path", shell_quote(package_path)},
        {"build_type", shell_quote(options.build_type)},
        {"flags", shell_quote(flags)},
        {"output", output_path.empty() ? "" : shell_quote(output_path)},
        {"workspace", shell_quote(workspace.name)}
    };

    if(builder) {
        std::string builder_source = builder->source_path;
        if(!builder_source.empty()) {
            builder_source = prefer_host_path(vfs, builder_source);
        }
        vars["builder"] = shell_quote(builder->id);
        vars["builder_path"] = builder_source.empty()
            ? shell_quote(builder->id)
            : shell_quote(builder_source);
    } else {
        vars["builder"] = "''";
        vars["builder_path"] = "''";
    }

    std::string working_dir;
    if(!base_dir.empty()) {
        working_dir = base_dir;
    } else if(!pkg.path.empty()) {
        auto parent = pkg_path_fs.parent_path();
        if(parent.empty()) {
            working_dir = ".";
        } else {
            working_dir = prefer_host_path(vfs, parent.lexically_normal().string());
        }
    } else {
        working_dir = ".";
    }

    std::string command_body;
    if(builder) {
        auto tpl = builder->get("COMMAND");
        if(tpl) {
            command_body = render_command_template(*tpl, vars);
        }
    }

    bool has_real_command = true;
    if(command_body.empty()) {
        has_real_command = false;
        std::string builder_label = builder ? builder->id : "<default>";
        std::string message = "upp.wksp.build: builder '" + builder_label +
                              "' has no COMMAND defined; configure the build method to describe how to build package '" +
                              pkg.name + "'.\n";
        command_body = "printf '%s' " + shell_quote(message) + " >&2; exit 1";
    }

    if(has_real_command && !output_path.empty()) {
        std::filesystem::path out_path(output_path);
        auto parent = out_path.parent_path();
        if(!parent.empty()) {
            command_body = "mkdir -p " + shell_quote(parent.string()) + " && " + command_body;
        }
    }

    return "cd " + shell_quote(working_dir) + " && " + command_body;
}

} // namespace

WorkspaceBuildSummary build_workspace(UppAssembly& assembly,
                                      Vfs& vfs,
                                      const WorkspaceBuildOptions& options) {
    WorkspaceBuildSummary summary;

    auto workspace = assembly.get_workspace();
    if(!workspace) {
        throw std::runtime_error("No active workspace. Use 'upp.wksp.open' first.");
    }

    std::shared_ptr<UppPackage> target_pkg;
    if(!options.target_package.empty()) {
        target_pkg = workspace->get_package(options.target_package);
        if(!target_pkg) {
            throw std::runtime_error("Target package not found in workspace: " + options.target_package);
        }
    } else {
        target_pkg = workspace->get_primary_package();
        if(!target_pkg) {
            throw std::runtime_error("Workspace has no primary package. Use 'upp.wksp.pkg.set' to choose one.");
        }
    }

    const UppBuildMethod* builder = nullptr;
    if(!options.builder_name.empty()) {
        builder = g_upp_builder_registry.get(options.builder_name);
        if(!builder) {
            throw std::runtime_error("Unknown builder: " + options.builder_name);
        }
    } else {
        builder = g_upp_builder_registry.active();
    }

    if(builder) {
        summary.builder_used = builder->id;
    } else {
        summary.builder_used = "<default>";
    }

    std::unordered_set<std::string> visiting;
    std::unordered_set<std::string> visited;
    collect_packages(workspace, target_pkg->name, visiting, visited, summary.package_order);

    BuildGraph plan;

    for(const auto& pkg_name : summary.package_order) {
        auto pkg = workspace->get_package(pkg_name);
        if(!pkg) continue;

        BuildRule rule;
        rule.name = package_target(pkg_name);
        rule.always_run = true;

        for(const auto& dep : pkg->dependencies) {
            if(workspace->get_package(dep)) {
                rule.dependencies.push_back(package_target(dep));
            }
        }

        BuildCommand cmd;
        cmd.type = BuildCommand::Type::Shell;
        cmd.text = make_command_for_package(*workspace, *pkg, options, vfs, builder);
        rule.commands.push_back(cmd);

        std::string output_path = default_output_path(*workspace, *pkg, options, vfs);
        if(!output_path.empty()) {
            rule.outputs.push_back(output_path);
        }

        plan.rules[rule.name] = rule;
    }

    summary.plan = plan;

    BuildOptions build_options;
    build_options.verbose = options.verbose;
    build_options.executor = [&](const BuildRule& rule, BuildResult& result, bool verbose) {
        if(options.dry_run) {
            for(const auto& cmd : rule.commands) {
                std::string line = "[dry-run] " + cmd.text + "\n";
                result.output += line;
            }
            return true;
        }
        return BuildGraph::runShellCommands(rule, result, verbose);
    };

    std::string target_name = package_target(target_pkg->name);
    summary.result = summary.plan.build(target_name, vfs, build_options);
    return summary;
}
