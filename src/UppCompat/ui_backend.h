#ifndef _UppCompat_ui_backend_h_
#define _UppCompat_ui_backend_h_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// Forward declarations
struct Vfs;

// UI Backend interface for U++ compatibility layer
namespace UiBackend {

// UI Component types
enum class ComponentType {
    WINDOW,
    PANEL,
    BUTTON,
    TEXT_INPUT,
    TEXT_AREA,
    LIST_BOX,
    TREE_VIEW,
    MENU_BAR,
    STATUS_BAR,
    TOOLBAR
};

// UI Event types
enum class EventType {
    CLICK,
    KEY_PRESS,
    MOUSE_MOVE,
    RESIZE,
    CLOSE,
    FOCUS_GAINED,
    FOCUS_LOST
};

// UI Component base class
struct UiComponent {
    std::string id;
    ComponentType type;
    std::map<std::string, std::string> properties;
    std::vector<std::shared_ptr<UiComponent>> children;
    
    UiComponent(const std::string& component_id, ComponentType comp_type)
        : id(component_id), type(comp_type) {}
    
    virtual ~UiComponent() = default;
    
    // Set property
    void setProperty(const std::string& key, const std::string& value);
    
    // Get property
    std::optional<std::string> getProperty(const std::string& key) const;
    
    // Add child component
    void addChild(std::shared_ptr<UiComponent> child);
    
    // Remove child component
    void removeChild(const std::string& child_id);
    
    // Find child by ID
    std::shared_ptr<UiComponent> findChild(const std::string& child_id);
};

// UI Event structure
struct UiEvent {
    EventType type;
    std::string component_id;
    std::map<std::string, std::string> data;
    
    UiEvent(EventType event_type, const std::string& comp_id)
        : type(event_type), component_id(comp_id) {}
};

// UI Backend interface
class UiBackend {
public:
    virtual ~UiBackend() = default;
    
    // Create a UI component
    virtual std::shared_ptr<UiComponent> createComponent(const std::string& id, ComponentType type) = 0;
    
    // Destroy a UI component
    virtual void destroyComponent(const std::string& id) = 0;
    
    // Find a UI component by ID
    virtual std::shared_ptr<UiComponent> findComponent(const std::string& id) = 0;
    
    // Set component property
    virtual void setComponentProperty(const std::string& id, const std::string& key, const std::string& value) = 0;
    
    // Get component property
    virtual std::optional<std::string> getComponentProperty(const std::string& id, const std::string& key) = 0;
    
    // Add event handler
    virtual void addEventHandler(const std::string& component_id, EventType event_type,
                                std::function<void(const UiEvent&)> handler) = 0;
    
    // Remove event handler
    virtual void removeEventHandler(const std::string& component_id, EventType event_type) = 0;
    
    // Process events
    virtual void processEvents() = 0;
    
    // Render UI
    virtual void render() = 0;
    
    // Show/hide component
    virtual void showComponent(const std::string& id, bool show = true) = 0;
    
    // Enable/disable component
    virtual void enableComponent(const std::string& id, bool enable = true) = 0;
};

// Global UI backend instance
extern std::unique_ptr<UiBackend> g_ui_backend;

// Initialize UI backend
bool initUiBackend();

// Shutdown UI backend
void shutdownUiBackend();

} // namespace UiBackend

#endif