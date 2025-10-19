#include "upp_assembly.h"
#include "VfsShell.h"  // Main header that includes all necessary definitions
#include "ui_backend.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string strip_trailing_delimiters(std::string value) {
    while(!value.empty() && (value.back() == ';' || value.back() == ',')) {
        value.pop_back();
    }
    return trim_copy(value);
}

std::string decode_upp_string_literal(const std::string& literal) {
    std::string text = trim_copy(literal);
    if(text.size() < 2 || text.front() != '"' || text.back() != '"') {
        return text;
    }

    std::string result;
    result.reserve(text.size() - 2);
    for(size_t i = 1; i + 1 < text.size(); ++i) {
        char ch = text[i];
        if(ch == '\\' && i + 1 < text.size() - 1) {
            char next = text[++i];
            if(next >= '0' && next <= '7') {
                std::string digits(1, next);
                size_t consumed = 1;
                while(i + 1 < text.size() - 1 &&
                      consumed < 3 &&
                      text[i + 1] >= '0' && text[i + 1] <= '7') {
                    digits.push_back(text[++i]);
                    ++consumed;
                }
                int value = std::strtol(digits.c_str(), nullptr, 8);
                result.push_back(static_cast<char>(value));
            } else {
                switch(next) {
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    case '\\': result.push_back('\\'); break;
                    case '"': result.push_back('"'); break;
                    default: result.push_back(next); break;
                }
            }
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

std::vector<std::string> split_upp_values(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    int paren_depth = 0;

    for(char ch : input) {
        if(ch == '"' && paren_depth >= 0) {
            in_quotes = !in_quotes;
            current.push_back(ch);
        } else if(!in_quotes && ch == '(') {
            ++paren_depth;
            current.push_back(ch);
        } else if(!in_quotes && ch == ')') {
            if(paren_depth > 0) --paren_depth;
            current.push_back(ch);
        } else if(!in_quotes && paren_depth == 0 && (ch == ';' || ch == ',')) {
            auto token = trim_copy(current);
            if(!token.empty()) tokens.push_back(token);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    auto token = trim_copy(current);
    if(!token.empty()) tokens.push_back(token);
    return tokens;
}

std::string clean_value_token(std::string token) {
    token = strip_trailing_delimiters(token);
    if(token.empty()) return token;

    if(token.front() == '(') {
        auto closing = token.find(')');
        if(closing != std::string::npos) {
            std::string after = trim_copy(token.substr(closing + 1));
            if(after.empty()) {
                token = trim_copy(token.substr(1, closing - 1));
            } else {
                token = after;
            }
        }
    }

    if(!token.empty() && token.front() == '"' && token.back() == '"') {
        return decode_upp_string_literal(token);
    }

    return token;
}

void handle_section_entry(const std::string& section,
                          const std::string& raw_entry,
                          UppPackage& pkg) {
    std::string entry = strip_trailing_delimiters(raw_entry);
    if(entry.empty()) return;

    auto values = split_upp_values(entry);
    if(values.empty()) return;

    if(section == "description") {
        pkg.description = decode_upp_string_literal(clean_value_token(values.front()));
        return;
    }

    if(section == "file") {
        for(auto& value : values) {
            std::string token = clean_value_token(value);
            if(!token.empty()) {
                pkg.files.push_back(token);
            }
        }
        return;
    }

    if(section == "uses" || section == "library" || section == "include") {
        for(auto& value : values) {
            std::string token = clean_value_token(value);
            if(token.empty()) continue;
            if(section == "library" || section == "include") {
                pkg.dependencies.push_back(token);
            } else {
                if(std::find(pkg.dependencies.begin(), pkg.dependencies.end(), token) == pkg.dependencies.end()) {
                    pkg.dependencies.push_back(token);
                }
            }
        }
        return;
    }

    // For now, other sections (mainconfig, options, etc.) are ignored.
}

bool is_comment_or_empty(const std::string& trimmed) {
    if(trimmed.empty()) return true;
    if(trimmed.rfind("//", 0) == 0) return true;
    if(trimmed[0] == '#') return true;
    return false;
}

} // namespace

// Implementation of UppWorkspace methods
void UppWorkspace::add_package(std::shared_ptr<UppPackage> pkg) {
    packages[pkg->name] = pkg;
    if (pkg->is_primary) {
        primary_package = pkg->name;
    }
}

std::shared_ptr<UppPackage> UppWorkspace::get_package(const std::string& name) {
    auto it = packages.find(name);
    if (it != packages.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<UppPackage> UppWorkspace::get_primary_package() {
    if (!primary_package.empty()) {
        return get_package(primary_package);
    }
    return nullptr;
}

std::vector<std::shared_ptr<UppPackage>> UppWorkspace::get_all_packages() {
    std::vector<std::shared_ptr<UppPackage>> result;
    for (const auto& pair : packages) {
        result.push_back(pair.second);
    }
    return result;
}

// Implementation of UppAssembly methods
UppAssembly::UppAssembly() {
    workspace = nullptr;
}

bool UppAssembly::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return load_from_content(buffer.str(), path);
}

bool UppAssembly::load_from_content(const std::string& content, const std::string& path) {
    return parse_var_content(content, path);
}

void UppAssembly::set_workspace(std::shared_ptr<UppWorkspace> ws) {
    workspace = ws;
}

std::shared_ptr<UppWorkspace> UppAssembly::get_workspace() {
    return workspace;
}

std::shared_ptr<UppPackage> UppAssembly::get_primary_package() {
    if (workspace) {
        return workspace->get_primary_package();
    }
    return nullptr;
}

std::vector<std::shared_ptr<UppPackage>> UppAssembly::get_all_packages() {
    if (workspace) {
        return workspace->get_all_packages();
    }
    return std::vector<std::shared_ptr<UppPackage>>();
}

bool UppAssembly::save_to_file(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << to_string();
    file.close();
    return true;
}

std::string UppAssembly::to_string() {
    if (!workspace) {
        return "";
    }
    
    std::stringstream ss;
    // Write assembly header
    ss << "// U++ Assembly File" << std::endl;
    ss << "// Generated by VfsShell" << std::endl;
    ss << std::endl;
    
    // Write workspace packages
    for (const auto& pkg_pair : workspace->packages) {
        auto pkg = pkg_pair.second;
        ss << "[Package: " << pkg->name << "]" << std::endl;
        ss << "Path=" << pkg->path << std::endl;
        ss << "Files=" << std::endl;
        for (const auto& file : pkg->files) {
            ss << "  " << file << std::endl;
        }
        ss << "Dependencies=" << std::endl;
        for (const auto& dep : pkg->dependencies) {
            ss << "  " << dep << std::endl;
        }
        if (pkg->is_primary) {
            ss << "Primary=true" << std::endl;
        }
        ss << std::endl;
    }
    
    return ss.str();
}

bool UppAssembly::parse_var_content(const std::string& content, const std::string& path) {
    // Create a new workspace
    std::string name = "default";
    if (!path.empty()) {
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos) {
            name = path.substr(last_slash + 1);
            // Remove extension
            size_t dot_pos = name.find_last_of('.');
            if (dot_pos != std::string::npos) {
                name = name.substr(0, dot_pos);
            }
        }
    }
    
    workspace = std::make_shared<UppWorkspace>(name, path);
    workspace->base_dir = path.substr(0, path.find_last_of('/'));
    
    std::istringstream stream(content);
    std::string line;
    std::shared_ptr<UppPackage> current_package = nullptr;
    
    while (std::getline(stream, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '/' || line[0] == '#') {
            continue;
        }
        
        // Trim leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Look for package section markers
        if (line.length() > 2 && line[0] == '[' && line[line.length()-1] == ']') {
            // Extract package name from [Package: Name]
            if (line.substr(0, 9) == "[Package:") {
                std::string package_name = line.substr(9, line.length() - 10);
                package_name.erase(0, package_name.find_first_not_of(" \t"));
                package_name.erase(package_name.find_last_not_of(" \t") + 1);
                
                // Create new package
                std::string package_path = workspace->base_dir + "/" + package_name;
                current_package = std::make_shared<UppPackage>(package_name, package_path);
                workspace->add_package(current_package);
            }
        } else if (current_package) {
            // Process package properties
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);
                
                // Trim key and value
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                
                if (key == "Path") {
                    current_package->path = value;
                } else if (key == "Primary") {
                    if (value == "true" || value == "1") {
                        current_package->is_primary = true;
                        workspace->primary_package = current_package->name;
                    }
                } else if (key == "Files") {
                    // In the .var format, files may be listed on following lines
                    // For now, we'll just mark that we expect file listings next
                    // Files are typically listed one per line with indentation
                } else if (key == "Dependencies") {
                    // Dependencies are typically listed one per line with indentation
                }
            } else if (line[0] == ' ' || line[0] == '\t') {
                // This is likely a file or dependency (indented line)
                std::string trimmed_line = line;
                trimmed_line.erase(0, trimmed_line.find_first_not_of(" \t"));
                
                if (!trimmed_line.empty()) {
                    // Determine if this is a file or dependency based on context
                    // For simplicity, we'll assume files are listed first, then dependencies
                    if (trimmed_line.substr(trimmed_line.length() - 2) == ".h" || 
                        trimmed_line.substr(trimmed_line.length() - 4) == ".cpp" ||
                        trimmed_line.substr(trimmed_line.length() - 4) == ".cc" ||
                        trimmed_line.substr(trimmed_line.length() - 2) == ".c") {
                        // It's a source file
                        current_package->files.push_back(trimmed_line);
                    } else {
                        // Assume it's a dependency
                        current_package->dependencies.push_back(trimmed_line);
                    }
                }
            }
        }
    }
    
    // If no primary package was explicitly set, set the first package as primary
    if (workspace->primary_package.empty() && !workspace->packages.empty()) {
        auto first_it = workspace->packages.begin();
        workspace->primary_package = first_it->first;
        first_it->second->is_primary = true;
    }
    
    return true;
}

bool UppAssembly::detect_packages_from_directory(Vfs& vfs, const std::string& base_path, bool verbose) {
    // Create a new workspace with a generic name based on the given path
    size_t last_slash = base_path.find_last_of('/');
    std::string workspace_name = "workspace";
    if (last_slash != std::string::npos && last_slash < base_path.length() - 1) {
        workspace_name = base_path.substr(last_slash + 1);
    }
    workspace = std::make_shared<UppWorkspace>(workspace_name, base_path);
    
    if (verbose) {
        std::cout << "Scanning directory: " << base_path << std::endl;
    }
    
    // First, check if the provided base_path itself is a U++ package
    std::string base_upp_file_path = base_path + "/" + workspace_name + ".upp";
    bool base_package_found = false;
    
    try {
        // Try to read the .upp file from the base path to see if it's a U++ package
        std::string upp_content = vfs.read(base_upp_file_path, std::nullopt);
        
        // If we successfully read the .upp file, this is a valid U++ package
        auto base_pkg = std::make_shared<UppPackage>(workspace_name, base_path, true); // Mark as primary initially
        
        // Parse the .upp file to get more information
        parse_upp_file_content(upp_content, *base_pkg);
        
        workspace->add_package(base_pkg);
        base_package_found = true;
        
        if (verbose) {
            std::cout << "Found U++ package in base directory: " << base_upp_file_path << std::endl;
        }
    } catch (...) {
        // The base path is not a direct U++ package, so search within it
        // This is fine, we'll look for packages inside instead
        if (verbose) {
            std::cout << "Base path is not a U++ package directory, searching subdirectories..." << std::endl;
        }
    }
    
    // List all directories in the base path to find potential packages
    try {
        // First, try to list through VFS
        auto overlay_ids = vfs.overlaysForPath(base_path);
        auto dir_listing = vfs.listDir(base_path, overlay_ids);
        
        // Process entries found through VFS
        for (const auto& [entry_name, entry] : dir_listing) {
            // Only process directories
            if (entry.types.count('d') > 0) {  // 'd' indicates directory
                std::string package_dir_path = base_path + "/" + entry_name;
                
                if (verbose) {
                    std::cout << "Visiting directory: " << package_dir_path << std::endl;
                }
                
                // Check if this directory contains a .upp file with the same name
                std::string upp_file_path = package_dir_path + "/" + entry_name + ".upp";
                
                if (verbose) {
                    std::cout << "Checking for U++ package file: " << upp_file_path << std::endl;
                }
                
                try {
                    // Try to read the .upp file to confirm it's a U++ package
                    std::string upp_content = vfs.read(upp_file_path, std::nullopt);
                    
                    // If we successfully read the .upp file, this is a valid U++ package
                    auto pkg = std::make_shared<UppPackage>(entry_name, package_dir_path);
                    
                    // Parse the .upp file to get more information
                    parse_upp_file_content(upp_content, *pkg);
                    
                    // If this package is the same as the base package, maintain it as primary
                    bool is_primary_pkg = (base_package_found && entry_name == workspace_name);
                    if (is_primary_pkg) {
                        pkg->is_primary = true;
                    }
                    
                    workspace->add_package(pkg);
                    
                    if (verbose) {
                        std::cout << "Found U++ package: " << upp_file_path << std::endl;
                    }
                    
                    // If no primary package is set yet and this isn't the base package, 
                    // set the first detected package as primary
                    if (!base_package_found && workspace->primary_package.empty()) {
                        pkg->is_primary = true;
                        workspace->primary_package = pkg->name;
                    }
                } catch (...) {
                    // If there's no .upp file, this is not a U++ package, continue
                    if (verbose) {
                        std::cout << "No U++ package file found in directory: " << package_dir_path << std::endl;
                    }
                    continue;
                }
            }
        }
        
        // If we still don't have any packages and the base_path wasn't a package, 
        // try to treat the base path as a package directory with a different name approach
        if (workspace->packages.empty()) {
            if (verbose) {
                std::cout << "No packages found in subdirectories, checking for .upp files in base directory..." << std::endl;
            }
            
            // Try to find any .upp file in the base directory
            auto base_dir_listing = vfs.listDir(base_path, overlay_ids);
            for (const auto& [entry_name, entry] : base_dir_listing) {
                if (entry.types.count('-') > 0) { // If it's a file (indicated by '-')
                    if (entry_name.length() > 4 && entry_name.substr(entry_name.length() - 4) == ".upp") {
                        std::string pkg_name = entry_name.substr(0, entry_name.length() - 4); // Remove .upp extension
                        
                        if (verbose) {
                            std::cout << "Found U++ package file in base directory: " << base_path + "/" + entry_name << std::endl;
                        }
                        
                        // Check if this package name matches directory name
                        std::string pkg_dir_path = base_path + "/" + pkg_name;
                        try {
                            // Try to read the .upp file
                            std::string upp_file_path = base_path + "/" + entry_name;
                            std::string upp_content = vfs.read(upp_file_path, std::nullopt);
                            
                            // Verify that the corresponding directory exists
                            auto dir_listing_check = vfs.listDir(pkg_dir_path, overlay_ids);
                            
                            // If directory exists, treat as valid package
                            auto pkg = std::make_shared<UppPackage>(pkg_name, pkg_dir_path);
                            
                            // Parse the .upp file to get more information
                            parse_upp_file_content(upp_content, *pkg);
                            
                            workspace->add_package(pkg);
                            
                            if (verbose) {
                                std::cout << "Added package from base directory: " << pkg_name << std::endl;
                            }
                            
                            // Make the first detected package primary
                            if (workspace->primary_package.empty()) {
                                pkg->is_primary = true;
                                workspace->primary_package = pkg->name;
                            }
                        } catch (...) {
                            // Skip if the directory doesn't exist or other issues occur
                            if (verbose) {
                                std::cout << "Could not process package directory: " << pkg_dir_path << ", skipping..." << std::endl;
                            }
                            continue;
                        }
                    }
                }
            }
        }
    } catch (...) {
        // If path doesn't exist or can't be listed, return false
        if (verbose) {
            std::cout << "Failed to list directory: " << base_path << std::endl;
        }
        return false;
    }
    
    if (verbose) {
        std::cout << "Finished scanning directory: " << base_path << ". Found " << workspace->packages.size() << " packages." << std::endl;
    }
    
    return true;
}

bool UppAssembly::parse_upp_file_content(const std::string& content, UppPackage& pkg) {
    std::istringstream stream(content);
    std::string raw_line;
    std::string current_section;

    while(std::getline(stream, raw_line)) {
        if(!raw_line.empty() && raw_line.back() == '\r') {
            raw_line.pop_back();
        }
        std::string trimmed = trim_copy(raw_line);
        if(is_comment_or_empty(trimmed)) continue;

        bool is_continuation = !raw_line.empty() && (raw_line[0] == ' ' || raw_line[0] == '\t');

        if(!is_continuation) {
            std::string clause = trimmed;
            std::string keyword;
            std::string rest;

            size_t space_pos = clause.find_first_of(" \t");
            size_t paren_pos = clause.find('(');
            bool paren_before_space = (paren_pos != std::string::npos) &&
                                      (space_pos == std::string::npos || paren_pos < space_pos);

            if(paren_before_space) {
                keyword = clause.substr(0, paren_pos);
                rest = clause.substr(paren_pos);
            } else if(space_pos != std::string::npos) {
                keyword = clause.substr(0, space_pos);
                rest = clause.substr(space_pos + 1);
            } else {
                keyword = clause;
            }

            keyword = to_lower_copy(trim_copy(keyword));
            rest = trim_copy(rest);

            if(!rest.empty() && rest.back() == ';') {
                rest.pop_back();
                rest = trim_copy(rest);
            }

            current_section.clear();

            if(keyword == "description" || keyword == "file" || keyword == "uses" ||
               keyword == "library" || keyword == "include") {
                current_section = keyword;
                if(!rest.empty()) {
                    handle_section_entry(current_section, rest, pkg);
                    if(keyword == "description") current_section.clear();
                }
            } else {
                current_section.clear();
            }
        } else if(!current_section.empty()) {
            handle_section_entry(current_section, trimmed, pkg);
        }
    }

    if(pkg.description.empty()) {
        pkg.description = pkg.name;
    }

    return true;
}

bool UppAssembly::build_project(const std::string& build_type, const std::string& output_path) {
    // For now, we'll just return true - the actual build implementation
    // would call the 'umk' command with appropriate parameters
    // This is a placeholder for future implementation
    
    // Example command that would be executed:
    // umk assembly_path,includes package_name build_type flags output_path
    
    return true;
}

void UppAssembly::create_vfs_structure(Vfs& vfs, size_t overlay_id) {
    if (!workspace) {
        return;
    }
    
    // Create /upp/ directory in VFS
    std::string upp_path = "/upp";
    vfs.ensureDir(upp_path, overlay_id);
    
    // Create workspace directory
    std::string ws_path = "/upp/" + workspace->name;
    vfs.ensureDir(ws_path, overlay_id);
    
    // Create packages directory
    std::string packages_path = ws_path + "/packages";
    vfs.ensureDir(packages_path, overlay_id);
    
    // Add each package
    for (const auto& pkg_pair : workspace->packages) {
        auto pkg = pkg_pair.second;
        
        // Create package directory
        std::string pkg_path = packages_path + "/" + pkg->name;
        vfs.ensureDir(pkg_path, overlay_id);
        
        // Add package files
        for (const auto& file : pkg->files) {
            std::string filename = file.substr(file.find_last_of("/\\") + 1);
            std::string content = "// U++ Package file: " + file + "\n";
            std::string file_path = pkg_path + "/" + filename;
            vfs.write(file_path, content, overlay_id);
        }
        
        // Add package metadata
        std::string metadata = "name=" + pkg->name + "\n";
        metadata += "path=" + pkg->path + "\n";
        metadata += "primary=" + std::string(pkg->is_primary ? "true" : "false") + "\n";
        metadata += "dependencies=" + std::accumulate(pkg->dependencies.begin(), pkg->dependencies.end(), 
                                                     std::string(), [](const std::string& a, const std::string& b) {
                                                         return a + (a.empty() ? "" : ",") + b;
                                                     }) + "\n";
        std::string metadata_path = pkg_path + "/package.info";
        vfs.write(metadata_path, metadata, overlay_id);
    }
    
    // Add workspace metadata
    std::string ws_metadata = "name=" + workspace->name + "\n";
    ws_metadata += "assembly_path=" + workspace->assembly_path + "\n";
    ws_metadata += "primary_package=" + workspace->primary_package + "\n";
    std::string ws_metadata_path = ws_path + "/workspace.info";
    vfs.write(ws_metadata_path, ws_metadata, overlay_id);
}

// Implementation of UppAssemblyGui methods
UppAssemblyGui::UppAssemblyGui(std::shared_ptr<UppAssembly> asm_ptr) 
    : assembly(asm_ptr) {
    main_editor = nullptr;
    package_list = nullptr;
    file_list = nullptr;
    menu_bar = nullptr;
    workspace = assembly ? assembly->get_workspace() : nullptr;
    
    // Initialize selections
    selected_package_idx = 0;
    selected_file_idx = 0;
    selected_line = 0;
    editor_offset = 0;
}

void UppAssemblyGui::init() {
    ui_init();
}

void UppAssemblyGui::run() {
    bool running = true;
    
    // Initialize initial state
    update_package_list();
    update_file_list();
    update_editor();
    
    while (running) {
        draw();
        ui_refresh();
        
        int ch = handle_input();
        
        // Handle keyboard shortcuts
        switch(ch) {
            case 'q':
            case 'Q':
            case 27:  // ESC key
                running = false;
                break;
            case 10:  // Enter key
                // For now, just refresh display
                break;
            case KEY_UP:
                if (selected_line > 0) selected_line--;
                break;
            case KEY_DOWN:
                // Will be handled in editor context
                break;
            case KEY_LEFT:
                // Switch focus between panels
                break;
            case KEY_RIGHT:
                // Switch focus between panels
                break;
            case KEY_F(1):
                // Show help
                show_help();
                break;
            case KEY_F(2):
                // Build project
                if (assembly) {
                    assembly->build_project();
                    // Show build status
                }
                break;
            case KEY_F(3):
                // Save workspace
                if (workspace && !workspace->assembly_path.empty()) {
                    assembly->save_to_file(workspace->assembly_path);
                }
                break;
            case KEY_F(10):
                // Exit
                running = false;
                break;
            default:
                // Process character input if focused on editor
                break;
        }
    }
    
    cleanup();
}

void UppAssemblyGui::draw() {
    ui_clear();
    
    int rows = ui_rows();
    int cols = ui_cols();
    
    // Calculate layout dimensions
    int menu_height = 1;
    int status_height = 1;
    int available_height = rows - menu_height - status_height;
    
    // Package list panel (left side, top portion)
    int pkg_list_height = available_height / 3;
    int pkg_list_width = cols / 4;  // 25% width
    int pkg_list_start_row = menu_height;
    
    // File list panel (left side, bottom portion)
    int file_list_start_row = pkg_list_start_row + pkg_list_height;
    int file_list_height = available_height - pkg_list_height;
    int file_list_width = pkg_list_width;
    
    // Main editor area (right side, takes remaining width)
    int editor_start_col = pkg_list_width;
    int editor_width = cols - pkg_list_width;
    int editor_start_row = menu_height;
    int editor_height = available_height;
    
    // Draw menu bar at the top (1 row)
    ui_move(0, 0);
    ui_color_on(UI_COLOR_BLUE);
    std::string menu_text = " U++ Assembly IDE - F1:Help F2:Build F3:Save F10:Exit ";
    // Truncate if too long
    if (static_cast<int>(menu_text.length()) >= cols) {
        menu_text = menu_text.substr(0, cols - 1);
    }
    ui_print_at(0, 0, menu_text.c_str());
    ui_color_off(UI_COLOR_BLUE);
    
    // Draw package list panel with border
    draw_panel(pkg_list_start_row, 0, pkg_list_height, pkg_list_width, "PACKAGES");
    
    // Draw file list panel with border
    draw_panel(file_list_start_row, 0, file_list_height, file_list_width, "FILES");
    
    // Draw main editor area with border
    draw_panel(editor_start_row, editor_start_col, editor_height, editor_width, "EDITOR");
    
    // Display packages in package list panel
    display_packages(pkg_list_start_row + 1, 1, pkg_list_height - 2, pkg_list_width - 2);
    
    // Display files in file list panel
    display_files(file_list_start_row + 1, 1, file_list_height - 2, file_list_width - 2);
    
    // Display editor content in main editor area
    display_editor_content(editor_start_row + 1, editor_start_col + 1, editor_height - 2, editor_width - 2);
    
    // Draw status line
    draw_status_bar(rows - 1, 0, cols);
}

void UppAssemblyGui::draw_panel(int start_row, int start_col, int height, int width, const std::string& title) {
    ui_color_on(UI_COLOR_CYAN);
    
    // Draw top border
    ui_move(start_row, start_col);
    ui_print("+");
    for (int i = 1; i < width - 1; i++) {
        ui_print("-");
    }
    if (width > 1) ui_print("+");
    
    // Draw side borders and title
    if (!title.empty()) {
        // Try to center the title in the top border
        int title_start = (width - static_cast<int>(title.length())) / 2;
        if (title_start < 1) title_start = 1;
        // Ensure the title doesn't go beyond the border
        if (title_start + static_cast<int>(title.length()) > width - 1) {
            title_start = width - static_cast<int>(title.length()) - 1;
        }
        if (title_start > 0) {
            ui_print_at(start_row, title_start, title.c_str());
        }
    }
    
    for (int r = start_row + 1; r < start_row + height - 1; r++) {
        if (width > 0) ui_print_at(r, start_col, "|");
        if (width > 1) ui_print_at(r, start_col + width - 1, "|");
    }
    
    // Draw bottom border
    if (height > 1) {
        ui_move(start_row + height - 1, start_col);
        ui_print("+");
        for (int i = 1; i < width - 1; i++) {
            ui_print("-");
        }
        if (width > 1) ui_print("+");
    }
    
    ui_color_off(UI_COLOR_CYAN);
}

void UppAssemblyGui::display_packages(int start_row, int start_col, int height, int width) {
    if (!workspace) return;
    
    auto packages = workspace->get_all_packages();
    
    for (int i = 0; i < height && i < static_cast<int>(packages.size()); i++) {
        std::string pkg_name = packages[i]->name;
        if (packages[i]->is_primary) {
            pkg_name = "*" + pkg_name;  // Mark primary package
        }
        
        // Add description if available
        if (!packages[i]->description.empty()) {
            pkg_name += " - " + packages[i]->description;
        }
        
        // Truncate if too long
        if (static_cast<int>(pkg_name.length()) >= width) {
            pkg_name = pkg_name.substr(0, width - 4) + "...";
        }
        
        // Highlight selected package
        if (i == selected_package_idx) {
            ui_color_on(UI_COLOR_YELLOW);
            ui_print_at(start_row + i, start_col, pkg_name.c_str());
            ui_color_off(UI_COLOR_YELLOW);
        } else {
            ui_print_at(start_row + i, start_col, pkg_name.c_str());
        }
    }
}

void UppAssemblyGui::display_files(int start_row, int start_col, int height, int width) {
    if (!workspace) return;
    
    std::shared_ptr<UppPackage> current_pkg = nullptr;
    auto packages = workspace->get_all_packages();
    if (!packages.empty() && selected_package_idx < static_cast<int>(packages.size())) {
        current_pkg = packages[selected_package_idx];
    } else if (!packages.empty()) {
        current_pkg = packages[0];  // Use first package if selection is invalid
    }
    
    if (!current_pkg) return;
    
    for (int i = 0; i < height && i < static_cast<int>(current_pkg->files.size()); i++) {
        std::string file_name = current_pkg->files[i];
        // Extract just the filename part
        size_t last_slash = file_name.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            file_name = file_name.substr(last_slash + 1);
        }
        
        // Add file extension indicator for different file types
        std::string display_name = file_name;
        if (file_name.substr(file_name.length() - 2) == ".h") {
            display_name = "[H] " + file_name;  // Header file
        } else if (file_name.substr(file_name.length() - 4) == ".cpp" || 
                   file_name.substr(file_name.length() - 4) == ".cxx" ||
                   file_name.substr(file_name.length() - 2) == ".c") {
            display_name = "[C] " + file_name;  // Source file
        } else {
            display_name = "[F] " + file_name;  // Other file
        }
        
        // Truncate if too long
        if (static_cast<int>(display_name.length()) >= width) {
            display_name = display_name.substr(0, width - 4) + "...";
        }
        
        // Highlight selected file
        if (i == selected_file_idx) {
            ui_color_on(UI_COLOR_YELLOW);
            ui_print_at(start_row + i, start_col, display_name.c_str());
            ui_color_off(UI_COLOR_YELLOW);
        } else {
            ui_print_at(start_row + i, start_col, display_name.c_str());
        }
    }
    
    // If no files, indicate that
    if (current_pkg->files.empty()) {
        ui_print_at(start_row, start_col, "[No files defined in .upp package]");
    }
}

void UppAssemblyGui::display_editor_content(int start_row, int start_col, int height, int width) {
    std::vector<std::string> lines;
    
    if (workspace) {
        std::shared_ptr<UppPackage> current_pkg = nullptr;
        auto packages = workspace->get_all_packages();
        if (!packages.empty() && selected_package_idx < static_cast<int>(packages.size())) {
            current_pkg = packages[selected_package_idx];
        } else if (!packages.empty()) {
            current_pkg = packages[0];  // Use first package if selection is invalid
        }
        
        if (current_pkg) {
            // Show package info header
            lines.push_back("// U++ Package: " + current_pkg->name);
            if (!current_pkg->description.empty()) {
                lines.push_back("// Description: " + current_pkg->description);
            }
            lines.push_back("// Path: " + current_pkg->path);
            lines.push_back("");
            
            // Show dependencies
            if (!current_pkg->dependencies.empty()) {
                lines.push_back("// Dependencies:");
                for (const auto& dep : current_pkg->dependencies) {
                    lines.push_back("//   - " + dep);
                }
                lines.push_back("");
            }
            
            // Show files
            if (!current_pkg->files.empty()) {
                lines.push_back("// Files:");
                for (const auto& file : current_pkg->files) {
                    lines.push_back("//   - " + file);
                }
                lines.push_back("");
            }
            
            // Show basic U++ template
            lines.push_back("#include <Core/Core.h>");
            lines.push_back("");
            lines.push_back("using namespace Upp;");
            lines.push_back("");
            
            // Show U++-specific code based on package name
            if (current_pkg->name.find("App") != std::string::npos) {
                // If package name suggests it's an app, add GUI_APP_MAIN
                lines.push_back("GUI_APP_MAIN");
                lines.push_back("{");
                lines.push_back("    // Application code for " + current_pkg->name);
                lines.push_back("    Cout() << \"Hello from " + current_pkg->name + "!\" << Endl;");
                lines.push_back("}");
            } else {
                // Otherwise, add a basic U++ module
                lines.push_back("void " + current_pkg->name + "_Initialize() {");
                lines.push_back("    // Initialization code");
                lines.push_back("}");
                lines.push_back("");
                lines.push_back("CONSOLE_APP_MAIN");
                lines.push_back("{");
                lines.push_back("    " + current_pkg->name + "_Initialize();");
                lines.push_back("    cout << \"Initialized " + current_pkg->name + "\\n\";");
                lines.push_back("}");
            }
        } else {
            lines.push_back("// No package selected");
            lines.push_back("// Use arrow keys to select a package from the left panel");
        }
    } else {
        lines.push_back("// No workspace loaded");
        lines.push_back("// Create a new workspace with 'upp.asm.create' command");
        lines.push_back("// Or scan for existing packages with 'upp.asm.scan' command");
    }
    
    // Display lines with scrolling
    int display_start = editor_offset;
    int lines_to_display = std::min(height, static_cast<int>(lines.size()) - display_start);
    
    for (int i = 0; i < lines_to_display && i < height; i++) {
        std::string line = lines[display_start + i];
        
        // Truncate line if it's too long for the display width
        if (static_cast<int>(line.length()) >= width) {
            line = line.substr(0, width - 1);
        }
        
        ui_print_at(start_row + i, start_col, line.c_str());
    }
    
    // Fill remaining space with tildes if we have fewer lines than available height
    for (int i = lines_to_display; i < height; i++) {
        ui_print_at(start_row + i, start_col, "~");
    }
    
    // Show position indicator if there's scrolling
    if (lines.size() > static_cast<size_t>(height)) {
        int pos = (editor_offset * height) / lines.size();
        if (pos >= height) pos = height - 1;
        ui_print_at(start_row + pos, start_col + width - 1, "|");
    }
}

void UppAssemblyGui::draw_status_bar(int row, int col, int width) {
    ui_color_on(UI_COLOR_YELLOW);
    
    std::string status = " U++ IDE | ";
    if (workspace) {
        status += "Workspace: " + workspace->name + " | ";
        if (auto primary = workspace->get_primary_package()) {
            status += "Primary: " + primary->name + " | ";
        }
    } else {
        status += "No workspace | ";
    }
    
    // Add current mode/focus info
    status += "PKG:" + std::to_string(selected_package_idx) + " ";
    status += "FILE:" + std::to_string(selected_file_idx) + " ";
    status += "LN:" + std::to_string(selected_line) + " ";
    status += "Press F10 to exit ";
    
    // Truncate if too long
    if (static_cast<int>(status.length()) >= width) {
        status = status.substr(0, width - 1);
    }
    
    ui_print_at(row, col, status.c_str());
    ui_color_off(UI_COLOR_YELLOW);
}

void UppAssemblyGui::show_help() {
    // Clear screen and show help text
    ui_clear();
    
    ui_move(0, 0);
    ui_color_on(UI_COLOR_BLUE);
    ui_print("U++ Assembly IDE Help");
    ui_color_off(UI_COLOR_BLUE);
    
    ui_move(2, 0);
    ui_print("Controls:");
    ui_move(3, 2);
    ui_print("Arrow Keys    - Navigate between elements");
    ui_move(4, 2);
    ui_print("F1            - Show this help");
    ui_move(5, 2);
    ui_print("F2            - Build project");
    ui_move(6, 2);
    ui_print("F3            - Save workspace");
    ui_move(7, 2);
    ui_print("F10 or ESC    - Exit IDE");
    ui_move(8, 2);
    ui_print("q             - Quit");
    
    ui_move(10, 0);
    ui_color_on(UI_COLOR_CYAN);
    ui_print("Layout:");
    ui_color_off(UI_COLOR_CYAN);
    ui_move(11, 2);
    ui_print("Top-left: Package list");
    ui_move(12, 2);
    ui_print("Bottom-left: File list for selected package");
    ui_move(13, 2);
    ui_print("Right: Main code editor");
    
    ui_move(15, 0);
    ui_print("Press any key to return to IDE...");
    ui_refresh();
    
    ui_getch(); // Wait for key press
}

int UppAssemblyGui::handle_input() {
    return ui_getch();
}

void UppAssemblyGui::update_package_list() {
    // No external changes needed for now - packages are updated when workspace changes
}

void UppAssemblyGui::update_file_list() {
    // No external changes needed for now - files are updated when package selection changes
}

void UppAssemblyGui::update_editor() {
    // For now, just ensure the editor content is ready for display
    // The actual display happens in display_editor_content
}

void UppAssemblyGui::cleanup() {
    ui_end();
}
