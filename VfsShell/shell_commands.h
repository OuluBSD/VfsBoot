#pragma once


// Forward declarations
struct Env;

// Shell command dispatcher
void dispatch_command(Vfs& vfs, std::shared_ptr<Env> env, const std::vector<std::string>& args);

// Tab completion
std::vector<std::string> get_all_command_names();
std::vector<std::string> get_vfs_path_completions(Vfs& vfs, const std::string& prefix);
