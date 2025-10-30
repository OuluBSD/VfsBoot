#ifndef _UppCompat_upp_builder_h_
#define _UppCompat_upp_builder_h_

#include <string>
#include <vector>
#include <map>
#include <optional>

// UppBuildMethod represents a U++ build method configuration
struct UppBuildMethod {
    std::string id;
    std::string source_path;
    std::string builder_type;
    std::map<std::string, std::string> properties;

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool has(const std::string& key) const;
    std::vector<std::string> keys() const;
    std::vector<std::string> splitList(const std::string& key, char delimiter = ' ') const;
};

// UppBuilderRegistry manages build methods
struct UppBuilderRegistry {
    std::map<std::string, UppBuildMethod> methods_;
    std::string active_;

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
};

// Utility function used by build method parser
std::string trim_copy(const std::string& s);

#endif
