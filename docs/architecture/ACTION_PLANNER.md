# Action Planner Test Suite

## Overview

The Action Planner Test Suite is a comprehensive system for building AI context from VFS nodes, filtering content based on tags and patterns, and testing code modification strategies. It serves as the **AI context offloader** mentioned in TASKS.md, providing hypothesis testing before calling AI.

## Architecture

### Core Components

1. **ContextFilter** - Flexible filtering system for selecting VFS nodes
2. **ContextBuilder** - Builds AI-ready context with token budget management
3. **ReplacementStrategy** - Determines what code to modify/replace
4. **ActionPlannerTestSuite** - Validates filtering and context building

## ContextFilter

Filter predicates for selecting VFS nodes based on various criteria.

### Filter Types

| Type | Description | Use Case |
|------|-------------|----------|
| `TagAny` | Node has any of specified tags | Find all nodes tagged "error-handling" OR "logging" |
| `TagAll` | Node has all specified tags | Find nodes tagged "critical" AND "tested" |
| `TagNone` | Node has none of specified tags | Exclude nodes tagged "deprecated" |
| `PathPrefix` | Path starts with prefix | Select all files in `/cpp/` |
| `PathPattern` | Path matches glob pattern | Select `*.cpp` files |
| `ContentMatch` | Content contains substring | Find files containing "TODO" |
| `ContentRegex` | Content matches regex | Find function definitions |
| `NodeKind` | Specific node type | Select only File nodes, exclude directories |
| `Custom` | User-defined lambda | Any custom logic |

### Factory Methods

```cpp
// Tag-based filters
ContextFilter::tagAny({tag_id1, tag_id2});
ContextFilter::tagAll({tag_id1, tag_id2});
ContextFilter::tagNone({deprecated_id});

// Path-based filters
ContextFilter::pathPrefix("/cpp/");
ContextFilter::pathPattern("*.cpp");

// Content-based filters
ContextFilter::contentMatch("TODO");
ContextFilter::contentRegex(R"(void\s+\w+\s*\()");

// Node kind filter
ContextFilter::nodeKind(VfsNode::Kind::File);

// Custom filter
ContextFilter::custom([](VfsNode* node) {
    return node->read().size() > 1000;
});
```

### Usage Example

```cpp
// Find all C++ files in /src with "error" in content
ContextBuilder builder(vfs, tag_storage, tag_registry, 4000);
builder.addFilter(ContextFilter::pathPrefix("/src/"));
builder.addFilter(ContextFilter::pathPattern("*.cpp"));
builder.addFilter(ContextFilter::contentMatch("error"));
builder.collect();
std::string context = builder.buildWithPriority();
```

## ContextBuilder

Builds AI context from VFS nodes with token budget management and smart prioritization.

### Features

- **Token Budget Management**: Respects maximum token limits (default 4000)
- **Priority System**: Higher priority nodes included first
  - `critical` tag: priority 200
  - `important` tag: priority 150
  - Default: priority 100
- **Overlay Support**: Handles multiple VFS overlays
- **Token Estimation**: ~4 chars per token (GPT-style)

### Methods

```cpp
ContextBuilder(Vfs& v, TagStorage& ts, TagRegistry& tr, size_t max_tokens = 4000);

void addFilter(const ContextFilter& filter);
void collect();                                    // Collect from /
void collectFromPath(const std::string& path);     // Collect from specific path
std::string build();                               // Build in insertion order
std::string buildWithPriority();                   // Build sorted by priority
size_t totalTokens() const;
size_t entryCount() const;
void clear();
```

### Shell Commands

```bash
# Build context from all VFS nodes
context.build [max_tokens]

# Configure filters (for scripting)
context.filter.tag <tag-name> [any|all|none]
context.filter.path <prefix-or-pattern>
```

### Example Session

```bash
> touch /src/main.cpp
> echo /src/main.cpp "int main() { return 0; }"
> tag.add /src/main.cpp important
> context.build 2000
=== Context Builder Results ===
Entries: 1
Total tokens: 8
Context (first 500 chars):
=== /src/main.cpp ===
Tags: important
int main() { return 0; }
```

## ReplacementStrategy

Determines what statements to remove or modify before adding new code.

### Strategy Types

| Type | Description | Parameters |
|------|-------------|------------|
| `ReplaceAll` | Replace entire file content | target_path, replacement |
| `ReplaceRange` | Replace line range | target_path, start_line, end_line, replacement |
| `ReplaceFunction` | Replace function definition | target_path, identifier, replacement |
| `InsertBefore` | Insert before matching line | target_path, match_pattern, replacement |
| `InsertAfter` | Insert after matching line | target_path, match_pattern, replacement |
| `DeleteMatching` | Delete lines matching pattern | target_path, match_pattern |
| `CommentOut` | Comment out matching lines | target_path, match_pattern |
| `ReplaceBlock` | Replace code block (TODO) | AST-based replacement |

### Factory Methods

```cpp
// Replace entire file
ReplacementStrategy::replaceAll("/cpp/test.cpp", "new content");

// Replace line range
ReplacementStrategy::replaceRange("/cpp/test.cpp", 10, 20, "// refactored code");

// Replace function
ReplacementStrategy::replaceFunction("/cpp/test.cpp", "oldFunc", "void newFunc() { ... }");

// Insert before/after
ReplacementStrategy::insertBefore("/cpp/test.cpp", "int main", "// Entry point");
ReplacementStrategy::insertAfter("/cpp/test.cpp", "#include", "#include <new_header>");

// Delete or comment out
ReplacementStrategy::deleteMatching("/cpp/test.cpp", "deprecated_function");
ReplacementStrategy::commentOut("/cpp/test.cpp", "FIXME");
```

### Example

```cpp
// Comment out all TODO lines in a file
auto strategy = ReplacementStrategy::commentOut("/src/main.cpp", "TODO");
bool success = strategy.apply(vfs);
```

## ActionPlannerTestSuite

Validates hypothesis without calling AI - use action planner's context builder to verify validity.

### Test Structure

```cpp
ActionPlannerTestSuite suite(vfs, tag_storage, tag_registry);

suite.addTest("test_name", "Description", []() -> bool {
    // Test logic
    return success;
});

suite.runAll();
suite.printResults();
```

### Included Tests

1. **tag_filter_any** - Verify TagAny filter matches nodes with specified tags
2. **path_prefix** - Verify path prefix filtering works correctly
3. **content_match** - Verify content substring matching
4. **context_builder_tokens** - Verify token budget limiting
5. **replacement_all** - Verify replaceAll strategy
6. **replacement_insert_before** - Verify insertBefore strategy

### Running Tests

```bash
> test.planner
=== Action Planner Test Results ===
✓ tag_filter_any
✓ path_prefix
✓ content_match
✓ context_builder_tokens
✓ replacement_all
✓ replacement_insert_before

Total: 6 tests, 6 passed, 0 failed
```

## Hypothesis Testing Workflow

The action planner follows the hypothesis testing approach from TASKS.md:

### 1. Simple Query Hypothesis
```bash
# Find function `foo` in VFS
context.filter.path "/astcpp/"
context.filter.content "foo"
context.build 1000
```

### 2. Code Modification Hypothesis
```bash
# Add error handling to function X
# Step 1: Find the function
context.filter.content "void functionX"
context.build 500

# Step 2: Identify insertion points
# (Manual inspection or AI-assisted)

# Step 3: Apply replacement
ReplacementStrategy::insertAfter("/src/main.cpp", "void functionX() {",
    "    if (error) return false;");
```

### 3. Refactoring Hypothesis
```bash
# Extract duplicated code
# Step 1: Tag duplicate blocks
tag.add /src/file1.cpp duplicate-block-A
tag.add /src/file2.cpp duplicate-block-A

# Step 2: Build context with duplicates
context.filter.tag duplicate-block-A any
context.build 2000

# Step 3: Generate helper function (AI or manual)
# Step 4: Replace duplicates with helper call
```

### 4. Feature Addition Hypothesis
```bash
# Add logging to error paths
# Step 1: Find all error returns
tag.add /src/foo.cpp error-return
tag.add /src/bar.cpp error-return

# Step 2: Build context
context.filter.tag error-return any
context.build 3000

# Step 3: Insert logging statements
ReplacementStrategy::insertBefore("/src/foo.cpp", "return false;",
    "    LOG_ERROR(\"Operation failed\");");
```

## Integration with AI Bridge

The context builder output is designed to be passed directly to AI:

```cpp
ContextBuilder builder(vfs, tag_storage, tag_registry, 4000);
builder.addFilter(ContextFilter::tagAny({critical_tag}));
builder.collect();
std::string context = builder.buildWithPriority();

// Build AI prompt
std::string prompt = "Given this code:\n" + context +
                     "\n\nTask: Add error handling to all critical functions.";
std::string response = call_ai(prompt);
```

## Token Budget Management

Token budgets prevent context overflow:

```cpp
// Small context for quick queries (1000 tokens ≈ 4000 chars)
ContextBuilder quick(vfs, tag_storage, tag_registry, 1000);

// Medium context for focused tasks (4000 tokens ≈ 16000 chars)
ContextBuilder medium(vfs, tag_storage, tag_registry, 4000);

// Large context for comprehensive analysis (8000 tokens ≈ 32000 chars)
ContextBuilder large(vfs, tag_storage, tag_registry, 8000);
```

## Future Enhancements

From TASKS.md:

1. **Logic-based tag system** with theorem proving for plan consistency
2. **Implication engine** for inferring missing tags automatically
3. **SAT/SMT integration** for complex constraint solving
4. **Learned patterns** from user feedback
5. **AST-based duplicate detection** for refactoring
6. **Automated comment generation** based on tags and context

## See Also

- [AGENTS.md](../AGENTS.md) - VfsShell architecture and agent specifications
- [README.md](../README.md) - Build instructions and usage
- [TASKS.md](../TASKS.md) - Current task priorities and completed work
