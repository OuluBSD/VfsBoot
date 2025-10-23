#!/usr/bin/env python3

# Read the entire file as a string
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'r') as f:
    content = f.read()

# Exact content to replace (copy exactly as it appears in the file)
old_exact = '''        if(builder) {
            std::string builder_label = builder->id;
            command_body = "printf '%s' \\"upp.wksp.build: builder '" + builder_label +
                          "' has no COMMAND defined; configure the build method to describe how to build package '" +
                          pkg.name + "'.'\\\\n\\\" >&2; exit 1";
        } else {'''

# New content
new_content_block = '''        if(builder) {
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
                command_body = "printf '%s' \\"upp.wksp.build: builder '" + builder_label +
                              "' has no COMMAND defined; configure the build method to describe how to build package '" +
                              pkg.name + "'.'\\\\n\\\" >&2; exit 1";
            }
        } else {'''

# Do the replacement
if old_exact in content:
    updated_content = content.replace(old_exact, new_content_block)
    
    # Write the updated content back
    with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'w') as f:
        f.write(updated_content)
    
    print("Successfully updated umk.cpp with GCC/CLANG command generation logic")
else:
    print("Exact pattern not found.")
    # Print some context to troubleshoot
    start = content.find("if(builder) {")
    if start != -1:
        # Look for the next few lines
        start_context = content.rfind("\n", 0, start)  # Find previous newline
        end_context = content.find("\n        } else {", start)
        if end_context != -1:
            end_context = content.find("\n", end_context)  # Find the end of that line
            
        print("Context around the pattern:")
        print(repr(content[start_context:end_context+1]))