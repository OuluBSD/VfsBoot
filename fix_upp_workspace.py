#!/usr/bin/env python3

# Read the file
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/upp_workspace_build.cpp', 'r') as f:
    content = f.read()

# Define the old code block to replace (exact match from the original file)
old_block = '''        if(builder) {
            std::string builder_label = builder->id;
            command_body = "printf '%s' \\\\\\\"upp.wksp.build: builder '" + builder_label +
                          "' has no COMMAND defined; configure the build method to describe how to build package '" +
                          pkg.name + "'.'\\\\\\\" >&2; exit 1";
        } else {'''

# Define the new code block
new_block = '''        if(builder) {
            // Check if this is a GCC or CLANG builder that needs automatic command generation
            std::string builder_type = builder->builder_type;
            if(builder_type == "GCC" || builder_type == "CLANG") {
                // Use a default GCC/CLANG build command
                std::string output_dir = default_output_path(workspace, pkg, options, vfs);
                if (output_dir.empty()) {
                    output_dir = "./out/" + pkg.name;
                }
                command_body = "mkdir -p " + shell_quote(output_dir) + " && " +
                               "find " + shell_quote(pkg.path.empty() ? "." : pkg.path) + " -name \\"*.cpp\\" -type f | " +
                               "xargs -I {} c++ -std=c++17 {} -o " + shell_quote(output_dir + "/" + pkg.name);
            } else {
                // For non-GCC/CLANG builders, show the error as before
                std::string builder_label = builder->id;
                command_body = "printf \'%s\' \\\\\\\"upp.wksp.build: builder \\\'\" + builder_label +
                              "\\\' has no COMMAND defined; configure the build method to describe how to build package \\\'\" +
                              pkg.name + "\\\'.'\\\\\\\" >&2; exit 1";
            }
        } else {'''

# Replace the old block with the new block
new_content = content.replace(old_block, new_block)

# Write the updated content back to the file
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/upp_workspace_build.cpp', 'w') as f:
    f.write(new_content)

print("Successfully updated upp_workspace_build.cpp with GCC/CLANG command generation logic")