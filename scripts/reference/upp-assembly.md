# U++ Assembly (.var files) Reference

Complete reference for all `upp.*` commands in VfsShell.

## Table of Contents
- [Overview](#overview)
- [Command Reference](#command-reference)
- [Examples](#examples)

## Overview

The U++ assembly system provides support for Ultimate++ (U++) development workflow within VfsShell. It manages .var assembly files which define multi-package U++ projects with dependencies.

### Core Concepts

**Assembly**: A collection of packages defined in .var files that describe U++ projects
**Package**: A logical grouping of source files with dependencies on other packages
**Workspace**: The active U++ development environment with a primary package

### Assembly File Format (.var)

```
[Package: PackageName]
Path=package/source/path
Files=
  source1.cpp
  source2.h
Dependencies=
  Core
  CtrlCore
Primary=true  # For primary package
```

## Command Reference

### upp.create
**Syntax**: `upp.create <name> <output-path>`

**Description**: Create a new U++ assembly with a primary package

**Parameters**:
- `<name>`: Name of the assembly and primary package
- `<output-path>`: VFS path where assembly file will be saved

**Effects**:
- Creates new UppAssembly instance
- Adds primary package with default structure
- Saves assembly to specified file path
- Creates VFS structure under `/upp/<name>/`

**Examples**:
```bash
upp.create MyApp /tmp/myapp.var
upp.create WebServer /projects/webserver.var
```

---

### upp.load
**Syntax**: `upp.load <var-file-path>`

**Description**: Load an existing U++ assembly file

**Parameters**:
- `<var-file-path>`: Path to .var file to load

**Effects**:
- Parses .var file content
- Creates UppAssembly with workspace and packages
- Builds VFS structure representing the assembly
- Makes assembly available for listing and GUI

**Examples**:
```bash
upp.load /tmp/myapp.var
upp.load /projects/webserver.var
```

---

### upp.list
**Syntax**: `upp.list`

**Description**: List packages in currently loaded U++ assemblies

**Output**:
- Assembly names under `/upp/`
- Package names in each assembly
- Primary package marker (`*`) for primary packages

**Example**:
```bash
upp.list
# Output:
# U++ Assembly packages:
# - MyApp
#   * MyApp (primary)
# - WebServer
#   * WebServer (primary)
#   Core
#   CtrlCore
```

---

### upp.gui
**Syntax**: `upp.gui`

**Description**: Launch U++ assembly IDE GUI

**Features**:
- Main code editor area (right side)
- Package list at top left
- File list for active package at bottom left
- Menu bar at top with shortcuts
- Keyboard navigation (arrow keys, F1-F10)

**Shortcuts**:
- F1: Show help
- F2: Build project (placeholder)
- F3: Save workspace
- F10: Exit GUI
- ESC/q: Exit GUI

**Effects**:
- Displays ncurses-based IDE interface
- Interactive package/file navigation
- Basic code editing capabilities

**Example**:
```bash
upp.gui
# Opens graphical interface for U++ development
```

---

## Examples

### Complete U++ Project Example

```bash
# 1. Create a new U++ assembly
upp.create MyWebApp /tmp/mywebapp.var

# 2. List to verify creation
upp.list

# 3. Load the assembly (if needed)
upp.load /tmp/mywebapp.var

# 4. List again to confirm
upp.list

# 5. Open GUI for development
# upp.gui  # Uncomment for interactive GUI
```

### Multi-Package Assembly Example

```bash
# 1. Create a complex assembly with dependencies
echo /tmp/complex.var "// Complex U++ Assembly File
[Package: MyApp]
Path=MyApp/src
Primary=true
Files=
  main.cpp
  MyApp.h
  MyApp.cpp
Dependencies=
  CtrlCore
  Core

[Package: CtrlCore]
Path=upp/CtrlCore
Dependencies=
  Core

[Package: Core]
Path=upp/Core
Dependencies=
  std
"

# 2. Load the complex assembly
upp.load /tmp/complex.var

# 3. Verify packages loaded
upp.list

# 4. Now you can use upp.gui to work with the project
```

### Development Workflow Example

```bash
# 1. Start a new project
upp.create QuickApp /projects/quickapp.var

# 2. Check the initial structure
upp.list

# 3. Develop using the GUI interface
# upp.gui

# 4. Create another project
upp.create SecondApp /projects/secondapp.var

# 5. Verify both assemblies are available
upp.list

# 6. Load a specific assembly to work on it
upp.load /projects/quickapp.var
```

---

## Best Practices

1. **Start with upp.create**: Always begin with a new assembly using `upp.create`
2. **Use descriptive names**: Choose clear names for assemblies and packages
3. **Maintain .var files**: Keep assembly files in VFS for persistence
4. **Use upp.list frequently**: Check your workspace state regularly
5. **Combine with GUI**: Use `upp.gui` for visual development workflow
6. **Save assemblies**: Store .var files in persistent locations for later loading

## Integration with VFS

The U++ system creates the following VFS structure:
```
/upp/
  /<assembly-name>/
    /packages/
      /<package-name>/
        package.info
        <source files...>
    workspace.info
```

Each package gets its own directory with metadata and source files. The workspace.info contains overall workspace configuration.

## GUI Layout

The U++ GUI features:
- **Top-left**: Package list panel
- **Bottom-left**: File list for selected package  
- **Right side**: Main code editing area
- **Top**: Menu bar with common actions
- **Bottom**: Status bar with current context

The interface is designed for efficient U++ development with keyboard navigation.