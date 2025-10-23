#!/usr/bin/env python3

import re

# Read the file
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/upp_workspace_build.cpp', 'r') as f:
    content = f.read()

# Find the specific function implementation
# Look for the make_command_for_package function and the specific if(builder) block
start_func = content.find('make_command_for_package')
if start_func == -1:
    print("Function not found")
    exit(1)

# Find the problematic section more specifically
# Find the part around line with "builder_label" and "has no COMMAND defined"
# We'll look for the specific section to replace
pattern_start = "has_real_command = false;"
pattern_end = "pkg.name + \"'.'\\\\\\\\\\\\\" >&2; exit 1\";"

# Find the beginning of the section we want to replace
start_pos = content.find(pattern_start)
if start_pos == -1:
    print("Start pattern not found")
    exit(1)

# Find the end of the problematic section
end_pos = content.find(pattern_end)
if end_pos == -1:
    print("End pattern not found")
    exit(1)

# Find the end of that line
end_pos = content.find('\n', end_pos)
if end_pos == -1:
    end_pos = len(content)

section_to_replace = content[start_pos:end_pos]

print("Section to replace:")
print(repr(section_to_replace))
print("\n---\n")

# The replacement code for GCC/CLANG
new_section = '''        has_real_command = false;
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
                command_body = "printf \'%s\' \\\\\\\"upp.wksp.build: builder \\\'\" + builder_label +
                              "\\\' has no COMMAND defined; configure the build method to describe how to build package \\\'\" +
                              pkg.name + "\\\'.'\\\\\\\" >&2; exit 1";
            }
        } else'''

print("New section:")
print(repr(new_section))
print("\n---\n")

# Replace the section
new_content = content.replace(section_to_replace, new_section)

# Write the file back
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/upp_workspace_build.cpp', 'w') as f:
    f.write(new_content)

print("Replacement in upp_workspace_build.cpp completed successfully!")