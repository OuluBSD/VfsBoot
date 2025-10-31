# VfsBoot User Manual

## Table of Contents
1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Core Commands](#core-commands)
4. [File System Operations](#file-system-operations)
5. [AI Integration](#ai-integration)
6. [VFS Overlays](#vfs-overlays)
7. [Tag System](#tag-system)
8. [Logic Engine](#logic-engine)
9. [Planning System](#planning-system)
10. [Hypothesis Testing](#hypothesis-testing)
11. [Feedback Pipeline](#feedback-pipeline)
12. [U++ Integration](#u-integration)
13. [Build Automation](#build-automation)
14. [Advanced Features](#advanced-features)
15. [Troubleshooting](#troubleshooting)

## Introduction

VfsBoot (Virtual File System Boot) is an advanced development environment that combines a virtualized filesystem with AI-assisted programming capabilities. It provides developers with tools for code manipulation, project planning, automated testing, and AI interaction through a sophisticated command-line interface.

The system features a hierarchical planning system, tag-based metadata management, logical inference engine, and integration with Ultimate++ (U++) development environment. It also includes support for AI-assisted development through the Qwen integration.

## Getting Started

### Basic Usage

Start VfsBoot by running the `vfsh` executable:

```
./vfsh
```

This enters the interactive shell where you can run various commands. The environment provides a welcome message and hints for getting started with AI discussions.

### Essential Commands

- `help` - Show all available commands
- `pwd` - Print working directory
- `ls [path]` - List directory contents
- `cd [path]` - Change directory
- `tree [path]` - Show directory tree structure
- `quit` or `exit` - Exit the shell

### Command Chains

VfsBoot supports command chaining similar to Unix shells:
- `a | b | c` - Pipe output of one command to the next
- `a && b` - Run b only if a succeeds
- `a || b` - Run b only if a fails

## Core Commands

### File Navigation and Information

- `pwd` - Print working directory and overlay information
- `cd [path]` - Change current working directory in the VFS
- `ls [path]` - List directory contents in the VFS
- `tree [path]` - Display directory tree structure
- `tree.adv [path]` - Advanced tree visualization with options:
  - `--no-box` - Don't use box characters
  - `--sizes` - Show file sizes
  - `--tags` - Show tags
  - `--colors` - Use colors
  - `--kind` - Show node kind
  - `--sort` - Sort entries
  - `--depth=N` - Limit tree depth
  - `--filter=pattern` - Filter entries by pattern

### File Operations

- `mkdir <path>` - Create directory
- `touch <path>` - Create empty file
- `cat [paths...]` - Display file contents (reads from stdin if no paths)
- `rm <path>` - Remove file or directory
- `mv <src> <dst>` - Move/rename file or directory
- `link <src> <dst>` - Create hard link
- `export <vfs> <host>` - Export VFS file to host filesystem
- `edit [file-path]` - Open full-screen text editor (also available as `ee`)

### Text Processing

- `grep [-i] <pattern> [path]` - Search for pattern in file
- `rg [-i] <pattern> [path]` - Search using regular expressions
- `head [-n N] [path]` - Display first N lines of file
- `tail [-n N] [path]` - Display last N lines of file
- `uniq [path]` - Remove duplicate consecutive lines
- `count [path]` - Count lines in file
- `random [min [max]]` - Generate random number in specified range
- `echo <data...>` - Print data to output

### History

- `history [-a | -n N]` - Show command history:
  - `-a` - Show all history
  - `-n N` - Show last N commands

### Boolean Operations

- `true` - Always succeeds
- `false` - Always fails

## File System Operations

### Mounting

VfsBoot supports several types of filesystem mounting:

- `mount <host-path> <vfs-path>` - Mount host filesystem to VFS location
- `mount.lib <lib-path> <vfs-path>` - Mount library to VFS location
- `mount.remote <host> <port> <remote-vfs-path> <local-vfs-path>` - Mount remote VFS
- `mount.list` - List all mounts
- `mount.allow` - Enable mounting
- `mount.disallow` - Disable mounting (existing mounts remain active)
- `unmount <vfs-path>` - Unmount a mounted filesystem

### Auto-loading

The system automatically loads `.vfs` files and `plan.vfs` during startup if present in the current directory.

## AI Integration

### Discussion System

The discussion system provides a multi-layered AI interaction framework:

- `discuss <message...>` - Natural language interaction with AI
  - Supports planning, execution, and simple query modes
  - Uses AI to generate plans, execute work, or answer questions
- `discuss.session new [name]` - Start new discussion session
- `discuss.session end` - End current discussion session

### AI Commands

- `ai <prompt...>` - Alias for discuss command
- `ai.raw <prompt>` - Call AI directly without discussion context
- `ai.brief <key> [extra...]` - Use predefined briefing templates
- `plan.answer <yes|no|explain> [reason...]` - Answer AI questions from discussion

### Tool Integration

- `tools` - List available tools for AI to use

## VFS Overlays

The overlay system allows multiple versions of files to coexist:

- `overlay.list` - List all overlays
- `overlay.mount <name> <file>` - Mount overlay file
- `overlay.save <name> <file>` - Save overlay to file
- `overlay.unmount <name>` - Unmount overlay
- `overlay.policy [manual|oldest|newest]` - Set conflict resolution policy
- `overlay.use <name>` - Set current overlay as primary

## Tag System

The tag system provides metadata and classification:

- `tag.add <vfs-path> <tag-name> [tag-name...]` - Add tags to file/directory
- `tag.remove <vfs-path> <tag-name> [tag-name...]` - Remove tags from file/directory
- `tag.list [vfs-path]` - List tags for path or all registered tags
- `tag.clear <vfs-path>` - Remove all tags from file/directory
- `tag.has <vfs-path> <tag-name>` - Check if file/directory has specific tag

### Tag Mining

- `tag.mine.start <tag> [tag...]` - Start mining session to understand tag relationships
- `tag.mine.feedback <tag-name> yes|no` - Provide feedback during mining
- `tag.mine.status` - Show current mining session status

## Logic Engine

The logic engine provides logical inference and consistency checking:

- `logic.init` - Initialize with hardcoded rules
- `logic.infer <tag> [tag...]` - Infer additional tags from provided ones
- `logic.check <tag> [tag...]` - Check for tag consistency
- `logic.explain <target-tag> <source-tag> [source-tag...]` - Explain how target is inferred
- `logic.listrules` - List all loaded logic rules
- `logic.sat <tag> [tag...]` - Check if tag formula is satisfiable

### Logic Rules Management

- `logic.rules.save [path]` - Save rules to VFS location
- `logic.rules.load [path]` - Load rules from VFS location
- `logic.rule.add <name> <premise-tag> <conclusion-tag> [confidence] [source]` - Add custom rule
- `logic.rule.exclude <name> <tag1> <tag2> [source]` - Add exclusion rule
- `logic.rule.remove <name>` - Remove rule by name

## Planning System

The planning system provides structured project management:

### Plan Creation

- `plan.create <path> <type> [content]` - Create new plan node
  - Types: `root`, `subplan`, `goals`, `ideas`, `strategy`, `jobs`, `deps`, `implemented`, `research`, `notes`
- `plan.goto <path>` - Navigate to plan location
- `plan.forward` - Switch to forward planning mode (adding details)
- `plan.backward` - Switch to backward planning mode (revising strategy)

### Plan Context Management

- `plan.context.add <vfs-path> [vfs-path...]` - Add paths to planning context
- `plan.context.remove <vfs-path> [vfs-path...]` - Remove paths from context
- `plan.context.clear` - Clear planning context
- `plan.context.list` - List paths in planning context
- `plan.status` - Show current planning status

### Plan Interaction

- `plan.discuss [message...]` - Interactive AI discussion about current plan
- `plan.verify [path]` - Verify tag consistency for plan
- `plan.tags.infer [path]` - Show complete inferred tag set for plan
- `plan.tags.check [path]` - Check for tag conflicts after inference
- `plan.validate [path]` - Recursively validate entire plan subtree
- `plan.save [file]` - Save plan tree to file

### Job Management

- `plan.jobs.add <jobs-path> <description> [priority] [assignee]` - Add job to jobs list
- `plan.jobs.complete <jobs-path> <index>` - Mark job as completed

### Plan Hypothesis

- `plan.hypothesis [type]` - Generate hypothesis for current plan

## Hypothesis Testing

The system includes 5 progressive levels of automated testing:

- `test.hypothesis` - Run all hypothesis tests
- `hypothesis.test <level> <goal> [description]` - Run custom hypothesis test (level 1-5)

### Hypothesis Levels

1. `hypothesis.query <target> [path]` - Level 1: Find pattern in codebase
2. `hypothesis.errorhandling <func> [style]` - Level 2: Add error handling to function
3. `hypothesis.duplicates [path] [min_lines]` - Level 3: Find duplicate code blocks
4. `hypothesis.logging [path]` - Level 4: Plan logging instrumentation
5. `hypothesis.pattern <pattern> [path]` - Level 5: Evaluate architecture pattern

## Feedback Pipeline

The automated feedback system helps evolve rules and improve the system:

- `feedback.metrics.show [top_n]` - Show metrics history
- `feedback.metrics.save [path]` - Save metrics to VFS
- `feedback.patches.list` - List pending rule patches
- `feedback.patches.apply [index|all]` - Apply patches
- `feedback.patches.reject [index|all]` - Reject patches
- `feedback.patches.save [path]` - Save patches to VFS
- `feedback.cycle [--auto-apply] [--min-evidence=N]` - Run full feedback cycle
- `feedback.review` - Interactive patch review

## U++ Integration

VfsBoot includes extensive integration with Ultimate++ (U++):

### Assembly Management

- `upp.asm.load <var-file-path> [-H]` - Load U++ assembly file (-H for host paths)
- `upp.asm.create <name> <output-path>` - Create new U++ assembly
- `upp.asm.list [-v]` - List packages in current assembly
- `upp.asm.scan <directory-path>` - Scan directory for U++ packages
- `upp.asm.load.host <host-var-file>` - Mount host directory and load .var from it
- `upp.asm.refresh [-v]` - Refresh all packages in active assembly
- `upp.gui` - Launch U++ assembly GUI

### Workspace Management

- `upp.wksp.open <pkg-name> [-v]` - Open package from list as workspace
- `upp.wksp.open -p <path> [-v]` - Open U++ package as workspace from path
- `upp.wksp.close` - Close current workspace
- `upp.wksp.pkg.list` - List packages in current workspace
- `upp.wksp.pkg.set <package-name>` - Set active package in workspace
- `upp.wksp.file.list` - List files in active package
- `upp.wksp.file.set <file-path>` - Set active file in editor
- `upp.wksp.file.edit [<file>]` - Open active file in editor
- `upp.wksp.file.cat` - Print active file content
- `upp.wksp.build [options]` - Build active workspace package

### Build Method Management

- `upp.builder.load <directory-path> [-H]` - Load all .bm files from directory
- `upp.builder.add <bm-file-path> [-H]` - Load single .bm build method file
- `upp.builder.list` - List loaded build methods
- `upp.builder.active.set <builder-name>` - Set active build method
- `upp.builder.get <key>` - Get key from active build method
- `upp.builder.set <key> <value>` - Update key in active build method
- `upp.builder.dump <builder-name>` - Dump keys for specific build method
- `upp.builder.active.dump` - Dump keys for active build method

### Startup Management

- `upp.startup.load <directory-path> [-H] [-v]` - Load all .var files from directory
- `upp.startup.list` - List all loaded startup assemblies
- `upp.startup.open <name> [-v]` - Load named startup assembly

## Build Automation

### Make System

- `make [target] [-f makefile] [-v|--verbose]` - Minimal GNU make subset implementation
- `sample.run [--keep] [--trace]` - Build, compile, and run demo C++ program

### C++ AST Operations

- `cpp.tu <ast-path>` - Create C++ translation unit
- `cpp.include <tu-path> <header> [angled0/1]` - Add include to translation unit
- `cpp.func <tu-path> <name> <ret>` - Create function node
- `cpp.param <fn-path> <type> <name>` - Add parameter to function
- `cpp.print <scope-path> <text>` - Add print statement
- `cpp.vardecl <scope-path> <type> <name> [init]` - Add variable declaration
- `cpp.expr <scope-path> <expression>` - Add expression statement
- `cpp.stmt <scope-path> <raw>` - Add raw statement
- `cpp.return <scope-path> [expression]` - Add return statement
- `cpp.returni <scope-path> <int>` - Add return integer statement
- `cpp.rangefor <scope-path> <loop-name> <decl> | <range>` - Add range-based for loop
- `cpp.dump <tu-path> <vfs-file-path>` - Generate C++ code from AST

## Advanced Features

### Qwen AI Assistant

- `qwen [options]` - Interactive AI assistant
- `qwen --attach <id>` - Attach to existing session
- `qwen --list-sessions` - List all sessions

### Context Building

- `context.build [max_tokens]` - Build AI context with token limit
- `context.build.adv [max_tokens] ...` - Advanced context building with options:
  - `--deps` - Include dependencies
  - `--dedup` - Deduplicate content
  - `--summary=N` - Use summaries for files with more than N lines
  - `--hierarchical` - Build hierarchical context
  - `--adaptive` - Use adaptive token budget
- `context.filter.tag <tag-name> [any|all|none]` - Add tag-based filter
- `context.filter.path <prefix-or-pattern>` - Add path-based filter

### Action Planner Tests

- `test.planner` - Run action planner test suite

### C++ Parsing with libclang

- `parse.file <filepath> [vfs-target-path]` - Parse C++ file with libclang
- `parse.dump [vfs-path]` - Dump parsed AST tree
- `parse.generate <ast-path> <output-path>` - Generate C++ code from AST

## Troubleshooting

### Common Issues

1. **Command not found**: Check that you're using the correct command name. Use `help` to see all available commands.

2. **Path not found**: Make sure the path exists in the VFS. Use `ls` and `pwd` to navigate.

3. **Overlay conflicts**: Use `overlay.list` to see current overlays and `overlay.policy` to set conflict resolution.

4. **AI not responding**: Ensure proper API keys are set in environment variables (OPENAI_API_KEY, etc.)

### Environment Variables

- `OPENAI_API_KEY` - API key for OpenAI integration
- `OPENAI_MODEL` - Model to use (default: gpt-4o-mini)
- `OPENAI_BASE_URL` - Base URL for OpenAI API (default: https://api.openai.com/v1)
- `LLAMA_BASE_URL` / `LLAMA_SERVER` - URL for Llama server (default: http://192.168.1.169:8080)
- `LLAMA_MODEL` - Model name for Llama server (default: coder)
- `CODEX_AI_PROVIDER` - Force specific AI provider (e.g. `llama`)
- `UPP` - Colon-separated list of U++ directories to auto-mount

### Startup Files

- `.vfshrc` - Configuration file loaded from current directory or `$HOME` on startup

### Script Execution

VfsBoot can execute scripts from files:
- `./vfsh <script>` - Execute script commands
- `./vfsh <script> -` - Execute script and return to interactive mode

### Solution Files

The system supports solution files with `.vfs` and `.var` extensions for project state persistence.

## See Also

### Tutorials
- [docs/tutorials/QWEN_MANAGER_TUTORIAL.md](docs/tutorials/QWEN_MANAGER_TUTORIAL.md) - Tutorial for qwen Manager mode

### How-To Guides
- [docs/howto/HOWTO_SCRIPTS.md](docs/howto/HOWTO_SCRIPTS.md) - Complete guide to running scripts
- scripts/tutorial/getting-started.cx - First steps tutorial
- scripts/examples/*.cx - Example scripts showing features

### Reference Documentation
- [docs/reference/QWEN_LOGGING_GUIDE.md](docs/reference/QWEN_LOGGING_GUIDE.md) - Logging configuration guide
- [docs/reference/ACCOUNTS_JSON_SPEC.md](docs/reference/ACCOUNTS_JSON_SPEC.md) - Schema specification for accounts configuration
- [docs/reference/MANAGER_PROTOCOL.md](docs/reference/MANAGER_PROTOCOL.md) - Communication protocol specification
- [QWEN.md](QWEN.md) - Complete qwen-code integration guide and protocol specification

### Architecture Documentation
- [docs/architecture/MODULAR_STRUCTURE.md](docs/architecture/MODULAR_STRUCTURE.md) - System modular structure
- [docs/architecture/MODULE_REFERENCE.md](docs/architecture/MODULE_REFERENCE.md) - Module reference documentation
- [docs/architecture/BITVECTOR_TAGSET.md](docs/architecture/BITVECTOR_TAGSET.md) - Tag system implementation details
- [docs/architecture/ACTION_PLANNER.md](docs/architecture/ACTION_PLANNER.md) - Action planner system architecture
- [docs/architecture/FEEDBACK_PIPELINE.md](docs/architecture/FEEDBACK_PIPELINE.md) - Feedback pipeline architecture