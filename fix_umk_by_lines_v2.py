#!/usr/bin/env python3

# Read the umk.cpp file as list of lines
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'r') as f:
    lines = f.readlines()

# Replace the block from lines 267-271 (0-indexed) which contains the original error code
new_block = [
    "        if(builder) {\n",
    "            // Check if this is a GCC or CLANG builder that needs automatic command generation\n",
    "            std::string builder_type = builder->builder_type;\n",
    "            if(builder_type == \"GCC\" || builder_type == \"CLANG\") {\n",
    "                // Use a default GCC/CLANG build command\n",
    "                std::string output_dir = default_output_path(workspace, pkg, options, vfs);\n",
    "                if (output_dir.empty()) {\n",
    "                    output_dir = \"./out/\" + pkg.name;\n",
    "                }\n",
    "                command_body = \"mkdir -p \" + shell_quote(output_dir) + \" && \" +\n",
    "                               \"find \" + shell_quote(pkg.path.empty() ? \".\" : pkg.path) + \" -name \\\"*.cpp\\\" -type f | \" +\n",
    "                               \"xargs -I {} c++ -std=c++17 {} -o \" + shell_quote(output_dir + \"/\" + pkg.name);\n",
    "            } else {\n",
    "                // For non-GCC/CLANG builders, show the error as before\n",
    "                std::string builder_label = builder->id;\n",
    "                command_body = \"printf '%s' \\\"upp.wksp.build: builder '\" + builder_label +\n",
    "                              \"' has no COMMAND defined; configure the build method to describe how to build package '\" +\n",
    "                              pkg.name + \"'.\\\\n\\\" >&2; exit 1\";\n",
    "            }\n",
    "        } else {\n"
]

# Replace the original block (lines 267-271) with the new block
start_line = 267
end_line = 271

# Get the prefix (before the block to be replaced)
prefix = lines[:start_line]

# Get the suffix (after the block to be replaced)
suffix = lines[end_line+1:]

# Combine prefix + new block + suffix
new_lines = prefix + new_block + suffix

# Write the updated content back to the file
with open('/common/active/sblo/Dev/VfsBoot/VfsShell/umk.cpp', 'w') as f:
    f.writelines(new_lines)

print("Successfully updated umk.cpp with GCC/CLANG command generation logic")