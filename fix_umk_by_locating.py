with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'r') as f:
    content = f.read()

# Find the block we need to change
start_marker = "has_real_command = false;"
end_marker = "} else {"

# Find the start of the block
start_pos = content.find(start_marker)
if start_pos != -1:
    # Find the end of the block (the matching closing brace before "} else {")
    # Count braces to find the right closing brace
    brace_count = 0
    pos = content.find('{', content.find('if(builder)', start_pos))
    
    if pos != -1:
        # Now count braces to find the matching closing brace
        search_pos = pos
        brace_count = 0
        
        while search_pos < len(content):
            if content[search_pos] == '{':
                brace_count += 1
            elif content[search_pos] == '}':
                brace_count -= 1
                if brace_count == 0:
                    # This is our target closing brace
                    end_brace_pos = search_pos
                    # Find the rest of the pattern until "} else {"
                    rest_pos = content.find("} else {", end_brace_pos)
                    if rest_pos != -1:
                        # Get the entire block to replace
                        block_start = content.rfind('\n', 0, start_pos) + 1  # Start from the beginning of the line
                        block_end = content.find('\n', rest_pos + len("} else {"))  # End after "} else {"
                        if block_end == -1:  # If it's at the end of file
                            block_end = len(content)
                        
                        old_block = content[block_start:block_end]
                        print(f"Found block to replace (start at line {content.count(chr(10), 0, block_start)}):")
                        print(repr(old_block))
                        
                        # New block
                        new_block = '''        has_real_command = false;
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
                command_body = "printf \'%s\' \\"upp.wksp.build: builder \\'" + builder_label +
                              "\\' has no COMMAND defined; configure the build method to describe how to build package \\'" +
                              pkg.name + "\\'."\\n\\" >&2; exit 1";
            }
        } else {'''
                        
                        # Replace the block
                        new_content = content.replace(old_block, new_block)
                        
                        # Write the file back
                        with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'w') as f:
                            f.write(new_content)
                        
                        print("Successfully updated umk.cpp")
                        break
            search_pos += 1
else:
    print("Start marker not found")