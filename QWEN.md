# Registry Implementation Documentation

## Overview

This document describes the implementation of a Windows Registry-like system for the codex VFS (Virtual File System). The registry provides a hierarchical key-value store that can be accessed through both filesystem paths (`/reg`) and dedicated shell commands.

## Architecture

### Core Components

1. **Registry Class**: The main registry manager that stores hierarchical key-value data
2. **RegistryKey Class**: Represents individual registry keys (folders) that contain values and subkeys
3. **VFS Integration**: Automatic synchronization between the registry and the VFS `/reg` directory

### Data Model

```
Registry Root (/)
├── Key: wksp
│   ├── Value: project_path = "/path/to/project"
│   ├── Value: last_opened = "2025-10-18T12:00:00"
│   └── Key: settings
│       ├── Value: theme = "dark"
│       └── Value: font_size = "12"
├── Key: user
│   ├── Value: name = "John Doe"
│   └── Value: email = "john@example.com"
└── Value: version = "1.0.0"
```

Each registry key can contain:
- **Values**: Key-value pairs where the value is stored as a string
- **Subkeys**: Nested registry keys for hierarchical organization

## Implementation Details

### File Structure

- `VfsShell/registry.h`: Header file with Registry and RegistryKey class definitions
- `VfsShell/registry.cpp`: Implementation of registry functionality
- `VfsShell/main.cpp`: Integration with the shell command system

### Key Methods

#### Registry Class

```cpp
// Set a registry value at the specified path
void setValue(const std::string& full_path, const std::string& data);

// Get a registry value at the specified path
std::string getValue(const std::string& full_path) const;

// Create a registry key at the specified path
void createKey(const std::string& full_path);

// List subkeys at the specified path
std::vector<std::string> listKeys(const std::string& path) const;

// List values at the specified path
std::vector<std::string> listValues(const std::string& path) const;

// Check if a registry key or value exists
bool exists(const std::string& path) const;

// Remove a registry key
void removeKey(const std::string& path);

// Remove a registry value
void removeValue(const std::string& path);

// Synchronize with VFS
void syncToVFS(Vfs& vfs, const std::string& registry_path = "/reg") const;
void syncFromVFS(Vfs& vfs, const std::string& registry_path = "/reg");
```

#### RegistryKey Class

```cpp
// Add a subkey
std::shared_ptr<RegistryKey> addSubKey(const std::string& name);

// Set a value
void setValue(const std::string& value_name, const std::string& data);

// Get a value
std::string getValue(const std::string& value_name) const;

// Check if a value exists
bool hasValue(const std::string& value_name) const;

// Check if a subkey exists
bool hasSubKey(const std::string& subkey_name) const;

// Get a subkey
std::shared_ptr<RegistryKey> getSubKey(const std::string& subkey_name) const;

// List subkeys and values
std::vector<std::string> listSubKeys() const;
std::vector<std::string> listValues() const;

// Navigate to a path
std::shared_ptr<RegistryKey> navigateTo(const std::string& path) const;
```

## Shell Commands

The registry system provides the following shell commands:

### `reg.set <key> <value>`
Sets a registry key to the specified value.

Example:
```bash
reg.set /wksp/project_path "/home/user/myproject"
reg.set /wksp/settings/theme "dark"
```

### `reg.get <key>`
Retrieves the value of a registry key.

Example:
```bash
reg.get /wksp/project_path
# Output: /home/user/myproject
```

### `reg.list [path]`
Lists registry keys and values at the specified path (defaults to root).

Example:
```bash
reg.list /wksp
# Output:
# settings/
# project_path
# last_opened
```

### `reg.rm <key>`
Removes a registry key or value.

Example:
```bash
reg.rm /wksp/old_setting
```

## VFS Integration

The registry automatically integrates with the VFS system through the `/reg` directory:

1. **Automatic Mounting**: The `/reg` directory is created and maintained automatically
2. **Bidirectional Sync**: Changes to registry values are reflected in the VFS filesystem and vice versa
3. **Persistent Storage**: Registry data can be persisted through VFS overlay files

### Filesystem Mapping

Registry paths map directly to VFS paths:

```
Registry Path        → VFS Path
/reg/wksp            → Directory: /reg/wksp/
/reg/wksp/project    → File: /reg/wksp/project (containing the value)
```

## Usage Examples

### Basic Operations

```bash
# Set values
reg.set /wksp/name "MyWorkspace"
reg.set /wksp/path "/home/user/projects"

# Retrieve values
reg.get /wksp/name
# Output: MyWorkspace

# List contents
reg.list /
# Output:
# wksp/

reg.list /wksp
# Output:
# name
# path

# Remove values
reg.rm /wksp/old_config
```

### Hierarchical Organization

```bash
# Create nested structure
reg.set /user/profile/name "John Doe"
reg.set /user/profile/email "john@example.com"
reg.set /user/preferences/theme "dark"
reg.set /user/preferences/font_size "12"

# List nested structure
reg.list /user
# Output:
# profile/
# preferences/

reg.list /user/profile
# Output:
# name
# email
```

### Integration with Existing VFS

The registry seamlessly integrates with the existing VFS system:

```bash
# Access registry values as files
cat /reg/user/profile/name
# Output: John Doe

# List registry structure with standard VFS commands
ls /reg/user/profile
# Output:
# name
# email

tree /reg
# Shows hierarchical registry structure
```

## Design Considerations

### Path Resolution

Registry paths follow standard filesystem conventions:
- Absolute paths start with `/`
- Relative paths are resolved from the current context
- Keys are represented as directories with a trailing `/`
- Values are represented as files without a trailing `/`

### Data Types

Currently, all registry values are stored as strings. Future enhancements could include:
- Integer values
- Boolean values
- Binary data (byte arrays)
- Lists and dictionaries

### Synchronization Strategy

The registry employs a bidirectional synchronization strategy:
1. **Write Operations**: When registry values are modified, they are immediately written to the corresponding VFS paths
2. **Read Operations**: Registry values are read directly from memory for optimal performance
3. **External Modifications**: Changes to VFS files are detected and synchronized with the registry

## Error Handling

The registry system follows these error handling principles:

1. **Graceful Degradation**: Errors during synchronization don't crash the system
2. **Informative Messages**: Clear error messages guide users toward solutions
3. **Recovery Mechanisms**: Automatic recovery from common error conditions

## Future Enhancements

Potential future improvements include:

1. **Data Type Support**: Native support for integers, booleans, and binary data
2. **Query Language**: SQL-like query capabilities for registry data
3. **Change Notifications**: Event system for registry changes
4. **Access Control**: Permission system for registry keys and values
5. **Backup/Restore**: Built-in backup and restore functionality
6. **Import/Export**: Support for importing/exporting registry data in standard formats

## Conclusion

The registry implementation provides a robust, Windows Registry-like system integrated with the codex VFS. It offers both programmatic access through shell commands and filesystem access through the `/reg` directory, making it flexible and familiar to developers coming from Windows environments while maintaining seamless integration with the existing Unix-like VFS system.