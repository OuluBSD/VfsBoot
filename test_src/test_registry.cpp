#include "VfsShell/registry.h"
#include <iostream>

int main() {
    Registry registry;
    
    // Test basic functionality
    registry.setValue("/wksp/test", "test_value");
    std::string value = registry.getValue("/wksp/test");
    std::cout << "Value: " << value << std::endl;
    
    // Test listing
    auto keys = registry.listKeys("/");
    std::cout << "Keys in root: ";
    for (const auto& key : keys) {
        std::cout << key << " ";
    }
    std::cout << std::endl;
    
    return 0;
}