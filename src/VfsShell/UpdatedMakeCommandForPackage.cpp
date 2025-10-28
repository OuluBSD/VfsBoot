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
        // Use internal U++ builder when no COMMAND is provided
        if(builder) {
            // Check if this is a GCC or CLANG builder that needs automatic command generation
            std::string builder_type = builder->builder_type;
            if(builder_type == "GCC" || builder_type == "CLANG") {
                // Generate automatic command for GCC or CLANG builders
                UppToolchain toolchain;
                toolchain.initFromBuildMethod(*builder, vfs);
                
                // Build the command with common parts
                std::string output_dir = default_output_path(workspace, pkg, options, vfs);
                if (output_dir.empty()) {
                    output_dir = "./out/" + pkg.name;
                }
                
                // Build compiler command
                std::string compiler_cmd = toolchain.compiler;
                
                // Add common compiler flags
                auto compile_flags = toolchain.effectiveCompileFlags(options.build_type);
                for (const auto& flag : compile_flags) {
                    compiler_cmd += " " + flag;
                }
                
                // Add include directories
                for (const auto& inc : toolchain.include_dirs) {
                    compiler_cmd += " -I" + shell_quote(inc);
                }
                
                // Build linker command
                std::string linker_cmd = toolchain.linker;
                
                // Add linker flags
                auto link_flags = toolchain.effectiveLinkFlags(options.build_type);
                for (const auto& flag : link_flags) {
                    linker_cmd += " " + flag;
                }
                
                // Add library directories
                for (const auto& lib : toolchain.library_dirs) {
                    linker_cmd += " -L" + shell_quote(lib);
                }
                
                // Discover sources
                std::vector<std::string> sources = toolchain.discoverSources(pkg.path.empty() ? "." : pkg.path);
                
                // If no sources found in package path, search in the current directory
                if (sources.empty() || sources[0] == "./main.cpp") {
                    sources.clear();
                    // Simple source discovery - look for all .cpp files
                    command_body = "mkdir -p " + shell_quote(output_dir) + " && " +
                                   "find " + shell_quote(pkg.path.empty() ? "." : pkg.path) + " -name \"*.cpp\" -type f -print0 | " +
                                   "xargs -0 " + compiler_cmd + " -o " + shell_quote(output_dir + "/" + pkg.name);
                } else {
                    // Use discovered sources
                    std::string source_list;
                    for (const auto& src : sources) {
                        if (!source_list.empty()) source_list += " ";
                        source_list += shell_quote(src);
                    }
                    command_body = "mkdir -p " + shell_quote(output_dir) + " && " +
                                   compiler_cmd + " " + source_list + " -o " + shell_quote(output_dir + "/" + pkg.name);
                }
            } else {
                // For non-GCC/CLANG builders, show the error as before
                std::string builder_label = builder->id;
                command_body = "printf '%s' \\\\\\\"upp.wksp.build: builder '" + builder_label +
                              "' has no COMMAND defined; configure the build method to describe how to build package '" +
                              pkg.name + "'.'\\\\\\\" >&2; exit 1";
            }
        } else {
            // Generate internal build command
            command_body = generate_internal_upp_build_command(workspace, pkg, options, vfs, builder);
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