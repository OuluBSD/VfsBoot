#ifndef _Qwen_registry_h_
#define _Qwen_registry_h_

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <functional>
#include <optional>

// Forward declarations
struct Vfs;

// QwenRegistry for managing named services and components
struct QwenRegistry {
    std::map<std::string, std::shared_ptr<void>> services;
    std::map<std::string, std::function<std::shared_ptr<void>()>> factories;
    
    // Register a service instance
    template<typename T>
    void registerService(const std::string& name, std::shared_ptr<T> service) {
        services[name] = std::static_pointer_cast<void>(service);
    }
    
    // Register a factory function for creating services
    template<typename T>
    void registerFactory(const std::string& name, std::function<std::shared_ptr<T>()> factory) {
        factories[name] = [factory]() -> std::shared_ptr<void> {
            return std::static_pointer_cast<void>(factory());
        };
    }
    
    // Get a service by name
    template<typename T>
    std::shared_ptr<T> getService(const std::string& name) {
        auto it = services.find(name);
        if (it != services.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        
        // Try to create from factory
        auto factory_it = factories.find(name);
        if (factory_it != factories.end()) {
            auto service = std::static_pointer_cast<T>(factory_it->second());
            services[name] = std::static_pointer_cast<void>(service);
            return service;
        }
        
        return nullptr;
    }
    
    // Check if a service is registered
    bool hasService(const std::string& name) const {
        return services.find(name) != services.end() || factories.find(name) != factories.end();
    }
    
    // Get a value from the registry (for string-based configuration)
    std::optional<std::string> getValue(const std::string& key) const {
        // For now, we'll just return nullopt since we don't have a key-value store implemented
        // In a real implementation, this would look up values in a configuration store
        return std::nullopt;
    }
    
    // Set a value in the registry (for string-based configuration)
    void setValue(const std::string& key, const std::string& value) {
        // For now, this is a no-op since we don't have a key-value store implemented
        // In a real implementation, this would store values in a configuration store
    }
    
    // List all registered services
    std::vector<std::string> listServices() const {
        std::vector<std::string> result;
        for (const auto& pair : services) {
            result.push_back(pair.first);
        }
        for (const auto& pair : factories) {
            if (services.find(pair.first) == services.end()) {
                result.push_back(pair.first);
            }
        }
        return result;
    }
    
    // Clear all services
    void clear() {
        services.clear();
        factories.clear();
    }
};

// Global registry instance
extern QwenRegistry g_registry;

#endif