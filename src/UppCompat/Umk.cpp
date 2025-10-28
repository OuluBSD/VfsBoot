#include "umk.h"
#include "VfsShell.h"
#include "upp_workspace_build.h"
#include "upp_builder.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <filesystem>

// Global U++ builder registry
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
                                       const UppBuildOptions& options,
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

std::string umk_flags(const UppBuildOptions& options, bool verbose) {
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
                                const UppBuildOptions& options,
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

std::string generate_internal_upp_build_command(const UppWorkspace& workspace,
                                               const UppPackage& pkg,
                                               const UppBuildOptions& options,
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
        // Use internal U++ builder when no COMMAND is provided
        if(builder) {
            // Check if this is a GCC or CLANG builder that needs automatic command generation
            std::string builder_type = builder->builder_type;
            if(builder_type == "GCC" || builder_type == "CLANG") {
                // Use a default GCC/CLANG build command
                std::string output_dir = default_output_path(workspace, pkg, options, vfs);
                if (output_dir.empty()) {
                    output_dir = "./out/" + pkg.name;
                }
                command_body = "mkdir -p " + shell_quote(output_dir) + " && " +
                               "find " + shell_quote(pkg.path.empty() ? "." : pkg.path) + " -name \"*.cpp\" -type f | " +
                               "xargs -I {} c++ -std=c++17 {} -o " + shell_quote(output_dir + "/" + pkg.name);
            } else {
                // For non-GCC/CLANG builders, show the error as before
                std::string builder_label = builder->id;
                command_body = "printf '%s' \"upp.wksp.build: builder \'" + builder_label +
                              "\' has no COMMAND defined; configure the build method to describe how to build package \'" +
                              pkg.name + "\'."\n\" >&2; exit 1";
            }
        } else {
            // Generate internal build command
            command_body = "echo 'Using internal U++ builder for " + pkg.name + "' && "
                         "mkdir -p " + shell_quote(output_path.empty() ? "./out/" + pkg.name : output_path) + " && "
                         "find . -name \"*.cpp\" -type f | "
                         "xargs -I {} c++ -std=c++17 -O2 -c {} -o " + shell_quote(output_path.empty() ? "./out/" + pkg.name : output_path) + "/$(basename {} .cpp).o && "
                         "find " + shell_quote(output_path.empty() ? "./out/" + pkg.name : output_path) + " -name \"*.o\" -type f | "
                         "xargs c++ -std=c++17 -O2 -o " + shell_quote(output_path.empty() ? "./out/" + pkg.name + "/" + pkg.name : output_path);
        }
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

// Implementation of UppToolchain
UppToolchain::UppToolchain() 
    : compiler("c++"), linker("c++") {
}

bool UppToolchain::initFromBuildMethod(const UppBuildMethod& method, Vfs& vfs) {
    // Get compiler
    auto compiler_opt = method.get("COMPILER");
    if(compiler_opt) {
        compiler = *compiler_opt;
    }
    
    // Get linker
    auto linker_opt = method.get("LINKER");
    if(linker_opt) {
        linker = *linker_opt;
    } else {
        linker = compiler; // Default to compiler if no linker specified
    }
    
    // Get include directories
    auto includes_opt = method.get("INCLUDES");
    if(includes_opt) {
        auto include_list = method.splitList("INCLUDES", ';');
        for(const auto& inc : include_list) {
            if(!inc.empty()) {
                // Map to host path if possible
                if(auto host_path = vfs.mapToHostPath(inc)) {
                    include_dirs.push_back(*host_path);
                } else {
                    include_dirs.push_back(inc);
                }
            }
        }
    }
    
    // Get library directories
    auto libs_opt = method.get("LIBS");
    if(libs_opt) {
        auto lib_list = method.splitList("LIBS", ';');
        for(const auto& lib : lib_list) {
            if(!lib.empty()) {
                // Map to host path if possible
                if(auto host_path = vfs.mapToHostPath(lib)) {
                    library_dirs.push_back(*host_path);
                } else {
                    library_dirs.push_back(lib);
                }
            }
        }
    }
    
    // Get flag bundles
    std::vector<std::string> flag_keys = {"COMMON_OPTIONS", "DEBUG_OPTIONS", "RELEASE_OPTIONS", 
                                          "GUI_OPTIONS", "USEMALLOC_OPTIONS"};
    for(const auto& key : flag_keys) {
        auto flags_opt = method.get(key);
        if(flags_opt) {
            flag_bundles[key] = *flags_opt;
        }
    }
    
    return true;
}

std::vector<std::string> UppToolchain::effectiveCompileFlags(const std::string& build_type) const {
    std::vector<std::string> flags;
    
    // Add common options first
    auto common_it = flag_bundles.find("COMMON_OPTIONS");
    if(common_it != flag_bundles.end()) {
        flags.push_back(common_it->second);
    }
    
    // Add build-type specific options
    std::string type_key = (build_type == "release") ? "RELEASE_OPTIONS" : "DEBUG_OPTIONS";
    auto type_it = flag_bundles.find(type_key);
    if(type_it != flag_bundles.end()) {
        flags.push_back(type_it->second);
    }
    
    // Add GUI options if needed (for now we'll assume GUI is always needed)
    auto gui_it = flag_bundles.find("GUI_OPTIONS");
    if(gui_it != flag_bundles.end()) {
        flags.push_back(gui_it->second);
    }
    
    return flags;
}

std::vector<std::string> UppToolchain::effectiveLinkFlags(const std::string& build_type) const {
    // For now, link flags are the same as compile flags
    return effectiveCompileFlags(build_type);
}

std::vector<std::string> UppToolchain::discoverSources(const std::string& package_path) const {
    std::vector<std::string> sources;
    
    try {
        // Check if the path exists in VFS
        // This is a simplified implementation - in a full implementation,
        // we would traverse the VFS to find source files
        sources.push_back(package_path + "/main.cpp");
    } catch (...) {
        // If we can't list the directory, return empty vector
    }
    
    return sources;
}

std::string UppToolchain::expandVariables(const std::string& flags, 
                                         const std::map<std::string, std::string>& variables) const {
    std::string result = flags;
    
    // Simple variable expansion - replace ${VAR} or $(VAR) with values
    for(const auto& [var_name, var_value] : variables) {
        std::string var_pattern1 = "${" + var_name + "}";
        std::string var_pattern2 = "$(" + var_name + ")";
        
        size_t pos = 0;
        while((pos = result.find(var_pattern1, pos)) != std::string::npos) {
            result.replace(pos, var_pattern1.length(), var_value);
            pos += var_value.length();
        }
        
        pos = 0;
        while((pos = result.find(var_pattern2, pos)) != std::string::npos) {
            result.replace(pos, var_pattern2.length(), var_value);
            pos += var_value.length();
        }
    }
    
    return result;
}

// Main build function
UppBuildSummary build_upp_workspace(UppAssembly& assembly,
                                   Vfs& vfs,
                                   const UppBuildOptions& options) {
    UppBuildSummary summary;

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
        cmd.text = generate_internal_upp_build_command(*workspace, *pkg, options, vfs, builder);
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