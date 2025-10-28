#pragma once

#include "VfsShell.h"
#include <map>
#include <string>
#include <memory>

// Registry-like functionality for VFS
struct Registry {
    // Represents a registry key (directory) that contains values (files)
    struct RegistryKey : std::enable_shared_from_this<RegistryKey> {
        std::string key_name;
        std::map<std::string, std::string> values; // registry value name -> value data
        std::map<std::string, std::shared_ptr<RegistryKey>> subkeys;
        std::weak_ptr<RegistryKey> parent;

        RegistryKey(const std::string& name) : key_name(name) {}

        std::shared_ptr<RegistryKey> addSubKey(const std::string& name);
        void setValue(const std::string& value_name, const std::string& data);
        std::string getValue(const std::string& value_name) const;
        bool hasValue(const std::string& value_name) const;
        bool hasSubKey(const std::string& subkey_name) const;
        std::shared_ptr<RegistryKey> getSubKey(const std::string& subkey_name) const;
        
        // Navigation methods
        std::vector<std::string> listSubKeys() const;
        std::vector<std::string> listValues() const;
        std::shared_ptr<RegistryKey> navigateTo(const std::string& path) const;
    };

    std::shared_ptr<RegistryKey> root;

    Registry();
    
    // Main registry operations
    void setValue(const std::string& full_path, const std::string& data);
    std::string getValue(const std::string& full_path) const;
    void createKey(const std::string& full_path);
    std::vector<std::string> listKeys(const std::string& path) const;
    std::vector<std::string> listValues(const std::string& path) const;
    bool exists(const std::string& path) const;
    void removeKey(const std::string& path);
    void removeValue(const std::string& path);

    // Integration with VFS
    void integrateWithVFS(Vfs& vfs);
    void syncFromVFS(Vfs& vfs, const std::string& registry_path = "/reg");
    void syncToVFS(Vfs& vfs, const std::string& registry_path = "/reg") const;
    
private:
    std::pair<std::shared_ptr<RegistryKey>, std::string> parsePath(const std::string& full_path) const;
    void syncSubKeyToVFS(Vfs& vfs, std::shared_ptr<RegistryKey> reg_key, const std::string& vfs_path) const;
};