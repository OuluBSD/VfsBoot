#!/usr/bin/env python3

import re

# Read the file
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'r') as f:
    content = f.read()

# Define the pattern to match the error-generating code in umk.cpp
# This is the simpler version since the string format is different
pattern = r'has_real_command = false;\s*// Use internal U\+\+ builder when no COMMAND is provided\s*if\(builder\) \{\s*std::string builder_label = builder->id;\s*command_body = "printf \\'%s\\' \\\\\"upp.wksp.build: builder \\\'\\" \+ builder_label \+\s*"\\' has no COMMAND defined; configure the build method to describe how to build package \\\'\\" \+\s*pkg.name \+ "\\\'\\.\\\\n\\\\" >&2; exit 1";'

# The replacement code
replacement = '''        has_real_command = false;
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
                               "find " + shell_quote(pkg.path.empty() ? "." : pkg.path) + " -name \\"*.cpp\\" -type f | " +
                               "xargs -I {} c++ -std=c++17 {} -o " + shell_quote(output_dir + "/" + pkg.name);
            } else {
                // For non-GCC/CLANG builders, show the error as before
                std::string builder_label = builder->id;
                command_body = "printf \'%s\' \\\"upp.wksp.build: builder \\\'\" + builder_label +
                              "\\\' has no COMMAND defined; configure the build method to describe how to build package \\\'\" +
                              pkg.name + \"\\'.\\n\\\" >&2; exit 1";
            }
        } else {'''

# Replace the pattern
new_content = re.sub(pattern, replacement, content, flags=re.MULTILINE|re.DOTALL)

# Write the file back
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'w') as f:
    f.write(new_content)

print("Replacement in umk.cpp completed successfully!")