#ifndef STAGE1_SNIPPET_CATALOG_H
#define STAGE1_SNIPPET_CATALOG_H

#include <string>

namespace snippets {

// Configure the catalog from argv[0]; safe to call repeatedly.
void initialize(const char* argv0);

// Allow tests or callers to override the snippet directory.
void set_directory(std::string path);

// Resolve a snippet by key, using fallback when no file exists.
std::string get_or(const std::string& key, const std::string& fallback);

// Convenience helpers for well-known snippets.
std::string tool_list();
std::string ai_bridge_hello_briefing();

} // namespace snippets

#endif // STAGE1_SNIPPET_CATALOG_H
