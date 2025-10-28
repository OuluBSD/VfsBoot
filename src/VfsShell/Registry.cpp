#include "registry.h"
#include <sstream>
#include <algorithm>

Registry::Registry() {
    root = std::make_shared<RegistryKey>("ROOT");
}

std::shared_ptr<Registry::RegistryKey> Registry::RegistryKey::addSubKey(const std::string& name) {
    if (subkeys.find(name) == subkeys.end()) {
        auto new_key = std::make_shared<RegistryKey>(name);
        new_key->parent = shared_from_this();
        subkeys[name] = new_key;
    }
    return subkeys[name];
}

void Registry::RegistryKey::setValue(const std::string& value_name, const std::string& data) {
    values[value_name] = data;
}

std::string Registry::RegistryKey::getValue(const std::string& value_name) const {
    auto it = values.find(value_name);
    if (it != values.end()) {
        return it->second;
    }
    return ""; // Return empty string if value doesn't exist
}

bool Registry::RegistryKey::hasValue(const std::string& value_name) const {
    return values.find(value_name) != values.end();
}

bool Registry::RegistryKey::hasSubKey(const std::string& subkey_name) const {
    return subkeys.find(subkey_name) != subkeys.end();
}

std::shared_ptr<Registry::RegistryKey> Registry::RegistryKey::getSubKey(const std::string& subkey_name) const {
    auto it = subkeys.find(subkey_name);
    if (it != subkeys.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> Registry::RegistryKey::listSubKeys() const {
    std::vector<std::string> result;
    for (const auto& pair : subkeys) {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<std::string> Registry::RegistryKey::listValues() const {
    std::vector<std::string> result;
    for (const auto& pair : values) {
        result.push_back(pair.first);
    }
    return result;
}

std::shared_ptr<Registry::RegistryKey> Registry::RegistryKey::navigateTo(const std::string& path) const {
    if (path.empty() || path == "/") {
        return const_cast<RegistryKey*>(this)->shared_from_this();
    }

    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    
    // Handle leading slash
    std::string clean_path = path;
    if (!clean_path.empty() && clean_path[0] == '/') {
        clean_path = clean_path.substr(1);
    }
    
    while (std::getline(ss, item, '/')) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }
    
    std::shared_ptr<RegistryKey> current = const_cast<RegistryKey*>(this)->shared_from_this();
    
    for (const std::string& part : parts) {
        auto subkey = current->getSubKey(part);
        if (!subkey) {
            return nullptr; // Path doesn't exist
        }
        current = subkey;
    }
    
    return current;
}

std::pair<std::shared_ptr<Registry::RegistryKey>, std::string> Registry::parsePath(const std::string& full_path) const {
    if (full_path.empty() || full_path == "/") {
        return {root, ""};
    }
    
    std::string clean_path = full_path;
    if (!clean_path.empty() && clean_path[0] == '/') {
        clean_path = clean_path.substr(1);
    }
    
    // Find last slash to separate key path from value name
    size_t last_slash = clean_path.find_last_of('/');
    if (last_slash == std::string::npos) {
        // No slash, means it's a value in the root key
        return {root, clean_path};
    }
    
    std::string key_path = clean_path.substr(0, last_slash);
    std::string value_name = clean_path.substr(last_slash + 1);
    
    // Navigate to the key
    std::shared_ptr<RegistryKey> key = root->navigateTo(key_path);
    if (!key) {
        // Create the path if it doesn't exist
        std::vector<std::string> parts;
        std::stringstream ss(key_path);
        std::string item;
        
        while (std::getline(ss, item, '/')) {
            if (!item.empty()) {
                parts.push_back(item);
            }
        }
        
        key = root;
        for (const std::string& part : parts) {
            key = key->addSubKey(part);
        }
    }
    
    return {key, value_name};
}

void Registry::setValue(const std::string& full_path, const std::string& data) {
    auto [key, value_name] = parsePath(full_path);
    if (key) {
        key->setValue(value_name, data);
    }
}

std::string Registry::getValue(const std::string& full_path) const {
    auto [key, value_name] = parsePath(full_path);
    if (key) {
        return key->getValue(value_name);
    }
    return "";
}

void Registry::createKey(const std::string& full_path) {
    auto [key, value_name] = parsePath(full_path);
    if (key && !value_name.empty()) {
        // This means the path points to a value, not a key
        // So we create the path to the parent key and add the value
        std::string parent_path = full_path.substr(0, full_path.find_last_of('/'));
        auto parent_key = root->navigateTo(parent_path);
        if (!parent_key) {
            // Create parent path
            std::vector<std::string> parts;
            std::stringstream ss(parent_path);
            std::string item;
            
            while (std::getline(ss, item, '/')) {
                if (!item.empty()) {
                    parts.push_back(item);
                }
            }
            
            parent_key = root;
            for (const std::string& part : parts) {
                parent_key = parent_key->addSubKey(part);
            }
        }
    } else if (key) {
        // It's a key path, not a value path
        // In this case, we just need to make sure the key exists
        // The parsePath function already ensures the path exists
    }
}

std::vector<std::string> Registry::listKeys(const std::string& path) const {
    auto [key, value_name] = parsePath(path);
    if (key && value_name.empty()) {
        return key->listSubKeys();
    } else if (key) {
        // If value_name is not empty, it means this is pointing to a value
        // So we navigate to the parent key to list its subkeys
        std::string parent_path = path.substr(0, path.find_last_of('/'));
        if (parent_path.empty()) {
            return root->listSubKeys();
        }
        auto parent_key = root->navigateTo(parent_path);
        if (parent_key) {
            return parent_key->listSubKeys();
        }
    }
    return {};
}

std::vector<std::string> Registry::listValues(const std::string& path) const {
    auto [key, value_name] = parsePath(path);
    if (key) {
        return key->listValues();
    }
    return {};
}

bool Registry::exists(const std::string& path) const {
    auto [key, value_name] = parsePath(path);
    if (!key) return false;
    
    if (value_name.empty()) {
        // Check if it's a key
        return true; // If we got here, the key exists
    } else {
        // Check if it's a value
        return key->hasValue(value_name);
    }
}

void Registry::removeKey(const std::string& path) {
    if (path == "/" || path.empty()) {
        // Cannot remove root
        return;
    }
    
    std::string parent_path = path.substr(0, path.find_last_of('/'));
    std::string key_name = path.substr(path.find_last_of('/') + 1);
    
    if (parent_path.empty()) parent_path = "/";
    
    auto [parent_key, _] = parsePath(parent_path);
    if (parent_key) {
        parent_key->subkeys.erase(key_name);
    }
}

void Registry::removeValue(const std::string& path) {
    auto [key, value_name] = parsePath(path);
    if (key && !value_name.empty()) {
        key->values.erase(value_name);
    }
}

void Registry::integrateWithVFS(Vfs& vfs) {
    // Ensure the /reg directory exists in the VFS
    vfs.mkdir("/reg", 0);
    
    // Set up the registry structure
    if (!vfs.resolve("/reg").get()->isDir()) {
        throw std::runtime_error("Failed to create /reg directory");
    }
    
    // Sync from VFS to registry
    syncFromVFS(vfs);
}

void Registry::syncFromVFS(Vfs& vfs, const std::string& registry_path) {
    try {
        // Pass an empty vector for overlays
        std::vector<size_t> empty_overlays;
        auto dir_listing = vfs.listDir(registry_path, empty_overlays);
        
        for (const auto& [name, entry] : dir_listing) {
            for (size_t i = 0; i < entry.nodes.size(); ++i) {
                auto node = entry.nodes[i];
                if (node->isDir()) {
                    // Recursively sync subdirectories
                    std::string sub_path = registry_path + "/" + name;
                    auto subkey = root->addSubKey(name);
                    syncFromVFS(vfs, sub_path);
                } else {
                    // It's a file, add as a registry value
                    try {
                        std::string content = vfs.read(registry_path + "/" + name, std::nullopt);
                        setValue(name, content);
                    } catch (...) {
                        // File might not be readable, skip it
                    }
                }
            }
        }
    } catch (...) {
        // Directory may not exist, that's ok
    }
}

void Registry::syncToVFS(Vfs& vfs, const std::string& registry_path) const {
    // First ensure the registry path exists in VFS
    vfs.ensureDir(registry_path, 0);
    
    // Sync root level values
    for (const auto& [value_name, value_data] : root->values) {
        std::string full_path = registry_path + "/" + value_name;
        vfs.write(full_path, value_data, 0);
    }
    
    // Recursively sync subkeys
    for (const auto& [subkey_name, subkey] : root->subkeys) {
        std::string sub_path = registry_path + "/" + subkey_name;
        vfs.mkdir(sub_path, 0);
        
        // Add values in this subkey
        for (const auto& [value_name, value_data] : subkey->values) {
            std::string value_path = sub_path + "/" + value_name;
            vfs.write(value_path, value_data, 0);
        }
        
        // Recursively sync nested subkeys
        for (const auto& [nested_name, nested_key] : subkey->subkeys) {
            std::string nested_path = sub_path + "/" + nested_name;
            syncSubKeyToVFS(vfs, nested_key, nested_path);
        }
    }
}

// Helper method to recursively sync a subkey to VFS
void Registry::syncSubKeyToVFS(Vfs& vfs, std::shared_ptr<RegistryKey> reg_key, const std::string& vfs_path) const {
    vfs.mkdir(vfs_path, 0);
    
    // Add values in this key
    for (const auto& [value_name, value_data] : reg_key->values) {
        std::string value_path = vfs_path + "/" + value_name;
        vfs.write(value_path, value_data, 0);
    }
    
    // Recursively sync nested subkeys
    for (const auto& [subkey_name, subkey] : reg_key->subkeys) {
        std::string sub_path = vfs_path + "/" + subkey_name;
        syncSubKeyToVFS(vfs, subkey, sub_path);
    }
}