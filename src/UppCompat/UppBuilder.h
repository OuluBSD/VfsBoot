#ifndef _VfsShell_upp_builder_h_
#define _VfsShell_upp_builder_h_

#include <map>
#include <optional>
#include <string>
#include <vector>

// Represents a single U++ build method (.bm file)
struct UppBuildMethod {
    std::string id;             // Derived from filename stem (e.g. CLANG_O3)
    std::string builder_type;   // Value of BUILDER key (e.g. CLANG)
    std::string source_path;    // Original location (VFS or host path)
    std::map<std::string, std::string> properties;

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool has(const std::string& key) const;
    std::vector<std::string> keys() const;
    std::vector<std::string> splitList(const std::string& key, char delimiter = ';') const;
};

// Registry of build methods loaded during the session
class UppBuilderRegistry {
public:
    bool parseAndStore(const std::string& id,
                       const std::string& source_path,
                       const std::string& content,
                       std::string& error_message);

    bool has(const std::string& id) const;
    const UppBuildMethod* get(const std::string& id) const;
    UppBuildMethod* getMutable(const std::string& id);
    std::vector<std::string> list() const;

    bool setActive(const std::string& id, std::string& error_message);
    std::optional<std::string> activeName() const;
    UppBuildMethod* active();
    const UppBuildMethod* active() const;

private:
    std::map<std::string, UppBuildMethod> methods_;
    std::string active_;
};

extern UppBuilderRegistry g_upp_builder_registry;

#endif
