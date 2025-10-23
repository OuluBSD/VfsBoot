#!/usr/bin/env python3

# Read the umk.cpp file as list of lines
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'r') as f:
    lines = f.readlines()

# Replace specific lines around 268-271 (0-indexed: 267-270)
# First replace line 268 (0-indexed 267) which contains: std::string builder_label = builder->id;
lines[267] = "            // Check if this is a GCC or CLANG builder that needs automatic command generation\n"
lines[268] = "            std::string builder_type = builder->builder_type;\n"
lines[269] = "            if(builder_type == \"GCC\" || builder_type == \"CLANG\") {\n"
lines[270] = "                // Use a default GCC/CLANG build command\n"

# Then insert more lines shifting the rest
new_lines = []
for i, line in enumerate(lines):
    if i < 267:
        new_lines.append(line)
    elif i == 267:
        # Replace the content we already defined above
        new_lines.extend([lines[267], lines[268], lines[269], lines[270]])
    elif i == 271:  # This is the line with the error message
        # We need to replace this with the if/else structure
        new_lines.append("                std::string output_dir = default_output_path(workspace, pkg, options, vfs);\n")
        new_lines.append("                if (output_dir.empty()) {\n")
        new_lines.append("                    output_dir = \"./out/\" + pkg.name;\n")
        new_lines.append("                }\n")
        new_lines.append("                command_body = \"mkdir -p \" + shell_quote(output_dir) + \" && \" +\n")
        new_lines.append("                               \"find \" + shell_quote(pkg.path.empty() ? \".\" : pkg.path) + \" -name \\\"*.cpp\\\" -type f | \" +\n")
        new_lines.append("                               \"xargs -I {} c++ -std=c++17 {} -o \" + shell_quote(output_dir + \"/\" + pkg.name);\n")
        new_lines.append("            } else {\n")
        new_lines.append("                // For non-GCC/CLANG builders, show the error as before\n")
        new_lines.append("                std::string builder_label = builder->id;\n")
        new_lines.append("                command_body = \"printf '%s' \\\"upp.wksp.build: builder '\" + builder_label +\n")
        new_lines.append("                              \"' has no COMMAND defined; configure the build method to describe how to build package '\" +\n")
        new_lines.append("                              pkg.name + \"'.\\\\n\\\" >&2; exit 1\";\n")
        new_lines.append("            }\n")
    elif i >= 272:  # Skip the original error message and else line
        new_lines.append(line)

# Write the updated content back to the file
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'w') as f:
    f.writelines(new_lines)

print("Successfully updated umk.cpp with GCC/CLANG command generation logic")