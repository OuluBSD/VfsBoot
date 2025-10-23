# How to Run Scripts in VfsBoot

This guide explains how to run all the script files in the `scripts/` directory.

## Quick Start Guide

**New to VfsBoot?** Try these demos in order:

### Logic Rules System (Progressive Learning Path)

1. **EASIEST**: `logic-rules-simple-demo.cx` - Learn rule save/load in 5 steps
2. **INTERMEDIATE**: `logic-rules-persistence-demo.cx` - Complete persistence workflow
3. **ADVANCED**: `logic-rules-dynamic-creation-demo.cx` - Create custom rules at runtime
4. **MOST COMPLEX**: `logic-rules-from-scratch-demo.cx` - Build complete system without hardcoded rules

Each demo builds on the previous, showing progressively advanced features.

**Key Insight**: The system doesn't require hardcoded rules! You can build entire rule sets dynamically for any domain.

## Script Types

VfsBoot uses two types of script files:

1. **.cx files** - Shell scripts for human users
   - Contain shell commands, one per line
   - Can be piped directly to `./vfsh`
   - Syntax: plain shell commands (help, ls, mkdir, echo, parse, eval, cpp.*, etc.)

2. **.sexp files** - S-expression scripts for AI and automation
   - Structured data format for programmatic use
   - Contain (begin (cmd "command" "args") ...) forms
   - Primarily used by test harness and AI workflows
   - More complex to read but better for automation

**Philosophy**: `.cx` is for humans, `.sexp` is for AI and testing automation.

## Running .cx Scripts

Shell scripts (`.cx`) can be run directly by piping them to codex:

```bash
# Method 1: Pipe the script
cat scripts/examples/hello-world.cx | ./vfsh

# Method 2: Use shell redirection
./vfsh < scripts/examples/hello-world.cx

# Method 3: Using printf with specific commands
printf 'help\nls /\nexit\n' | ./vfsh
```

### Examples

**scripts/examples/hello-world.cx** - Basic S-expression evaluation:
```bash
cat scripts/examples/hello-world.cx | ./vfsh
```
This script:
- Shows help
- Writes S-expression to `/src/hello.cx`
- Parses it to `/ast/hello`
- Evaluates the lambda expression
- Output: `42`

**scripts/tutorial/getting-started.cx** - Tutorial walkthrough:
```bash
cat scripts/tutorial/getting-started.cx | ./vfsh
```
This script demonstrates:
- S-expression evaluation with `parse` and `eval`
- C++ code generation with `cpp.*` commands
- Tree visualization of VFS
- Output: Generated C++ file with hello world

**scripts/examples/mount-demo.cx** - Filesystem and library mounting:
```bash
cat scripts/examples/mount-demo.cx | ./vfsh
```
This script demonstrates:
- Mounting real filesystems into VFS
- Loading shared libraries
- Viewing mount points

**scripts/examples/remote-mount-demo.cx** - Remote VFS mounting:
```bash
# Start server first: ./vfsh --daemon 9999
cat scripts/examples/remote-mount-demo.cx | ./vfsh
```
This script demonstrates:
- Connecting to remote codex daemon
- Mounting remote VFS paths
- Accessing remote files over network

**scripts/examples/remote-copy-demo.cx** - Remote file copying:
```bash
# Start server first: ./vfsh --daemon 9999
cat scripts/examples/remote-copy-demo.cx | ./vfsh
```
This script demonstrates:
- Copying files between hosts via VFS
- Mounting local and remote filesystems
- Cross-network file operations

**scripts/examples/overlay-demo.cx** - Overlay system demo:
```bash
cat scripts/examples/overlay-demo.cx | ./vfsh
```
Shows how to:
- Mount and save overlays
- Work with multiple VFS layers
- List and manage overlays

**scripts/examples/action-planner-demo.cx** - Action planner and context building:
```bash
./vfsh scripts/examples/action-planner-demo.cx
```
Demonstrates:
- Tag-based filtering and context building
- Path and content filters
- Token budget management
- Code replacement strategies

**scripts/examples/tree-viz-context-demo.cx** - Advanced tree visualization:
```bash
./vfsh scripts/examples/tree-viz-context-demo.cx
```
Shows:
- Box-drawing character trees
- ANSI color coding
- Tag and size displays
- Advanced context builder with deduplication

**scripts/examples/hypothesis-testing-demo.cx** - Hypothesis testing system:
```bash
./vfsh scripts/examples/hypothesis-testing-demo.cx
```
Demonstrates 5 levels of hypothesis testing:
- Simple query (pattern matching)
- Code modification (error handling)
- Refactoring (duplicate detection)
- Feature addition (logging)
- Architecture (design patterns)

**scripts/examples/logic-system-demo.cx** - Logic system and tag mining:
```bash
./vfsh scripts/examples/logic-system-demo.cx
```
Demonstrates:
- Tag inference with forward chaining
- Consistency checking and conflict detection
- Tag mining workflow
- SAT solving for formula satisfiability
- Inference explanation

**scripts/examples/logic-plan-validation.cx** - Plan validation with logic system:
```bash
./vfsh scripts/examples/logic-plan-validation.cx
```
Real-world scenario for validating plan configurations:
- Detecting conflicting requirements (offline + network)
- Preventing mutually exclusive configurations (write-through + write-back)
- Automatic inference of missing tags (fast → cached)
- Interactive tag mining for plan refinement
- Building consistent plans before implementation

**scripts/examples/logic-debugging-session.cx** - Debugging with logic system:
```bash
./vfsh scripts/examples/logic-debugging-session.cx
```
Debugging workflow using tag logic:
- Root cause analysis of configuration conflicts
- Hypothesis validation with consistency checking
- Building diagnostic trees with tagged hypotheses
- Tracing implications through rule chains
- Finding impossible configurations before deployment

**scripts/examples/logic-rules-simple-demo.cx** - Simple rules save/load (EASIEST):
```bash
cat scripts/examples/logic-rules-simple-demo.cx | tail -n +2 | ./vfsh
```
Minimal introduction to rule persistence:
- Load hardcoded rules with `logic.init`
- Save rules to `/plan/rules` with `logic.rules.save`
- View saved files with `tree` and `cat`
- Load rules back with `logic.rules.load`
- **Perfect for first-time users**

**scripts/examples/logic-rules-persistence-demo.cx** - Rules persistence workflow (INTERMEDIATE):
```bash
cat scripts/examples/logic-rules-persistence-demo.cx | tail -n +2 | ./vfsh
```
Comprehensive persistence demonstration:
- Organizing rules by source (hardcoded/learned/user/ai-generated)
- Pipe-delimited serialization format
- Summary file generation
- Rule inference before and after reload
- Integration with VFS overlay system

**scripts/examples/logic-rules-advanced-demo.cx** - Advanced rule management (MOST COMPLEX):
```bash
cat scripts/examples/logic-rules-advanced-demo.cx | tail -n +2 | ./vfsh
```
Production-grade rule management workflow:
- Multi-source rule creation and management
- Custom user-defined rules with confidence scores
- Learned rules from simulated ML feedback
- Complex S-expression formulas (AND, OR, IMPLIES)
- Version management and snapshotting
- Consistency checking and SAT solving integration
- Rule provenance and metadata tracking
- Overlay integration for team collaboration
- Git-friendly text format for version control
- **Shows complete production workflow**

**scripts/examples/logic-rules-dynamic-creation-demo.cx** - Dynamic rule creation:
```bash
cat scripts/examples/logic-rules-dynamic-creation-demo.cx | tail -n +2 | ./vfsh
```
Demonstrates creating rules from multiple sources:
- `logic.rule.add` - Create simple implication rules (A → B)
- `logic.rule.exclude` - Create mutual exclusion rules (A → ¬B)
- `logic.rule.remove` - Remove and update existing rules
- User-defined domain rules with custom confidence
- Learned patterns from ML feedback (0.6-0.9 confidence)
- AI-generated suggestions from LLMs (0.7-0.8 confidence)
- Building complex inference chains
- Real-world ML training pipeline example
- **Shows how to go beyond hardcoded rules**

**scripts/examples/logic-rules-from-scratch-demo.cx** - Complete system without hardcoded rules:
```bash
cat scripts/examples/logic-rules-from-scratch-demo.cx | tail -n +2 | ./vfsh
```
Build a complete rule system from scratch (NO logic.init):
- 20-part comprehensive workflow (270+ lines)
- Web application deployment domain (50+ rules)
- Infrastructure rules (environments, scaling, load balancing)
- Security rules (HTTPS, encryption, authentication)
- Performance rules (caching, CDN, compression)
- Network topology rules (distributed, multi-region)
- Resource constraint modeling
- Learned patterns from deployment history
- AI optimization suggestions
- Production + staging + development scenarios
- Deployment workflow validation
- Team collaboration and versioning
- **Proves system works entirely without hardcoded rules**

**scripts/examples/full-integration-hello-world.cx** - FULL SYSTEM INTEGRATION:
```bash
./scripts/examples/full-integration-hello-world.cx
```
**THE ULTIMATE DEMO** - Shows all systems working together:
- **User request**: "ai create hello world program in c++"
- **Planning System**: 3-level hierarchy (root → subplans → jobs)
- **Logic Solver**: 13 rules with tag inference and constraint checking
- **Action Planner**: Context building with 3000 token budget
- **C++ AST Builder**: Translation unit → include → function → statements → dump
- **Hypothesis Testing**: Validation and verification
- **Result**: Working, compilable C++ "Hello World" program
- **10 phases**: initialization, planning, breakdown, task planning, context building, code generation, hypothesis testing, visualization, validation, persistence
- **Perfect capstone demo** showing the complete VfsBoot workflow from concept to code
- Demonstrates: How tags drive everything (user tags → logic inference → context filtering → code gen)
- Shows: Plans track execution (jobs created → executed with AST builder → marked complete)
- Validates: Throughout the pipeline (pre-planning logic.check → hypothesis testing → post plan.validate)
- Unifies: All data in VFS (plans at /plan, AST at /astcpp, code at /cpp, rules at /plan/rules)

**scripts/examples/ai-interactive-demo.cx** - Interactive AI Sub-Loop Demo:
```bash
./scripts/examples/ai-interactive-demo.cx
```
**NEW FEATURE** - Interactive multi-turn conversations:
- **Command**: `ai` (without arguments) enters interactive mode
- **Prompt**: Changes to `discuss>` for conversation mode
- **Command**: `plan.discuss` (without arguments) for plan-specific discussion
- **Prompt**: Changes to `plan.discuss>` with context display
- **Exit**: Type `exit`, `back`, or `quit` to return to main prompt
- **Old workflow**: Repeat `discuss <msg>` for each turn (repetitive)
- **New workflow**: Enter `ai` once, multiple turns, then `exit` (natural)
- **Features**: Custom prompts per mode, session preservation, independent history
- **Usage examples**: Shows before/after comparison and interactive test commands
- **Perfect for**: Extended AI conversations without command repetition

## Running .sexp Scripts

S-expression scripts are primarily used by the test harness, but can be run manually:

```bash
# Note: .sexp files contain structured data, not directly executable
# They're meant for the test harness or AI workflows

# View the structure
cat scripts/examples/ai-hello-world.sexp

# The test harness processes these automatically
python tools/test_harness.py scripts/examples/ai-hello-world.sexp
```

### .sexp File Structure

Example from `scripts/examples/ai-hello-world.sexp`:
```lisp
(begin
  (comment "AI-bridge demo: build hello world via cpp AST helpers")
  (cmd "tools")
  (cmd "mkdir" "/astcpp/bridge")
  (cmd "cpp.tu" "/astcpp/bridge/hello")
  (cmd "cpp.include" "/astcpp/bridge/hello" "iostream" 1)
  (cmd "cpp.func" "/astcpp/bridge/hello" "main" "int")
  (cmd "cpp.print" "/astcpp/bridge/hello/main/body" "hello from the ai bridge")
  (cmd "cpp.returni" "/astcpp/bridge/hello/main/body" 0)
  (cmd "cpp.dump" "/astcpp/bridge/hello" "/cpp/tests/ai-hello.cpp")
  (cmd "cat" "/cpp/tests/ai-hello.cpp")
)
```

To create a `.cx` version from a `.sexp` file, extract the commands:
```bash
# Convert ai-hello-world.sexp to executable commands
cat > /tmp/ai-hello-test.cx << 'EOF'
tools
mkdir /astcpp/bridge
cpp.tu /astcpp/bridge/hello
cpp.include /astcpp/bridge/hello iostream 1
cpp.func /astcpp/bridge/hello main int
cpp.print /astcpp/bridge/hello/main/body hello from the ai bridge
cpp.returni /astcpp/bridge/hello/main/body 0
cpp.dump /astcpp/bridge/hello /cpp/tests/ai-hello.cpp
cat /cpp/tests/ai-hello.cpp
exit
EOF

cat /tmp/ai-hello-test.cx | ./vfsh
```

## Script Directory Organization

```
scripts/
├── examples/          # Example scripts showing features
│   ├── hello-world.cx        # Basic S-expression demo
│   ├── ai-hello-world.sexp   # AI bridge with C++ AST
│   ├── mount-demo.cx         # Filesystem/library mounting demo
│   ├── overlay-demo.cx       # Overlay system demo
│   ├── remote-mount-demo.cx  # Remote VFS mounting demo
│   └── remote-copy-demo.cx   # Remote file copy demo
├── reference/         # Reference examples of commands
│   └── command-tour.cx       # Tour of all shell commands
├── tutorial/          # Learning progression
│   └── getting-started.cx    # First steps tutorial
└── unittst/           # Unit test scripts
    ├── core.cx              # Core functionality tests
    └── cpp-escape.cx        # C++ string escaping tests
```

## Running All Scripts

### Run all .cx scripts:
```bash
# Run all example scripts
for script in scripts/examples/*.cx; do
    echo "=== Running $script ==="
    cat "$script" | ./vfsh
    echo ""
done

# Run all tutorial scripts
for script in scripts/tutorial/*.cx; do
    echo "=== Running $script ==="
    cat "$script" | ./vfsh
    echo ""
done

# Run reference scripts
for script in scripts/reference/*.cx; do
    echo "=== Running $script ==="
    cat "$script" | ./vfsh
    echo ""
done

# Run unit test scripts
for script in scripts/unittst/*.cx; do
    echo "=== Running $script ==="
    cat "$script" | ./vfsh
    echo ""
done
```

### Run all automated tests (.sexp via test harness):
```bash
# Run all tests
python tools/test_harness.py

# Run specific directory
python tools/test_harness.py tests/

# Run with verbose output
python tools/test_harness.py -v
```

## Creating Your Own Scripts

### Create a .cx script:
```bash
cat > my-script.cx << 'EOF'
help
mkdir /src/myproject
echo /src/myproject/hello.cx (print "Hello from my script")
parse /src/myproject/hello.cx /ast/myproject-hello
eval /ast/myproject-hello
tree /
exit
EOF

cat my-script.cx | ./vfsh
```

### Create a .sexp script for testing:
See `tests/` directory for examples. Structure:
```lisp
(test
  (id "my-test-001")
  (title "My Test")
  (difficulty "easy")
  (tags "tutorial" "basic")
  (instructions
    (system "...")
    (task "..."))
  (prompt "...")
  (expect
    (contains "expected output"))
  (assertions
    (file-contains "/path/to/file" "expected content"))
  (targets "openai" "llama"))
```

## Common Patterns

### Pattern 1: Build and Export C++ Code
```bash
cat > build-cpp.cx << 'EOF'
cpp.tu /astcpp/myprogram
cpp.include /astcpp/myprogram iostream 1
cpp.func /astcpp/myprogram main int
cpp.print /astcpp/myprogram/main/body Hello from generated code
cpp.returni /astcpp/myprogram/main/body 0
cpp.dump /astcpp/myprogram /cpp/myprogram.cpp
export /cpp/myprogram.cpp ./myprogram.cpp
exit
EOF

cat build-cpp.cx | ./vfsh
c++ myprogram.cpp -o myprogram
./myprogram
```

### Pattern 2: Mount Host Filesystems
```bash
cat > mount-demo.cx << 'EOF'
mkdir /mnt
mkdir /dev
mount tests /mnt/tests
ls /mnt/tests
mount.lib tests/libtest.so /dev/testlib
cat /dev/testlib/_info
mount.list
unmount /mnt/tests
unmount /dev/testlib
exit
EOF

cat mount-demo.cx | ./vfsh
```

See `scripts/examples/mount-demo.cx` for a complete mounting example.

### Pattern 3: Work with Overlays
```bash
cat > overlay-work.cx << 'EOF'
overlay.mount mywork mywork.cxpkg
overlay.use mywork
mkdir /myproject
echo /myproject/readme.txt This is my project
overlay.save mywork mywork.cxpkg
overlay.list
exit
EOF

cat overlay-work.cx | ./vfsh
```

### Pattern 4: S-Expression Programming
```bash
cat > lisp-demo.cx << 'EOF'
echo /src/fib.cx (lambda n (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
parse /src/fib.cx /ast/fib
eval /ast/fib
exit
EOF

cat lisp-demo.cx | ./vfsh
```

## Interactive Mode

You can also run scripts interactively:

```bash
./vfsh

# Then type commands manually:
codex> help
codex> ls /
codex> mkdir /src/test
codex> echo /src/test/hello.cx (print "Hello")
codex> parse /src/test/hello.cx /ast/test-hello
codex> eval /ast/test-hello
codex> exit
```

## Tips

1. **End with `exit`**: Always end .cx scripts with `exit` to cleanly terminate
2. **Check errors**: codex will print errors to stderr - check return codes
3. **Use tree**: `tree /` shows the full VFS structure after script execution
4. **Export files**: Use `export` to save VFS files to host filesystem
5. **Test incrementally**: Build scripts incrementally, testing each step

## Debugging Scripts

```bash
# Add tree and ls commands to see VFS state
cat my-script.cx | ./vfsh

# Use verbose test harness
python tools/test_harness.py my-test.sexp -v

# Enable tracing (requires rebuild with -DCODEX_TRACE)
make clean
make CXXFLAGS="-O0 -g -DCODEX_TRACE"
cat my-script.cx | ./vfsh
cat codex_trace.log
```

## Planner System Scripts

The planner system provides hierarchical project planning with task tracking, context management, and persistence.

### Running Planner Examples

**scripts/examples/plan-basic.cx** - Basic planner usage:
```bash
cat scripts/examples/plan-basic.cx | ./vfsh
```
This script demonstrates:
- Creating a project plan with goals, strategy, and jobs
- Adding jobs with priorities and assignees
- Managing dependencies and research topics
- Saving plans to plan.vfs

**scripts/examples/plan-navigation.cx** - Navigation and context:
```bash
cat scripts/examples/plan-navigation.cx | ./vfsh
```
This script shows:
- Navigating through hierarchical plans
- Forward/backward mode switching
- Managing AI context (visible nodes)
- Using plan.status to check state

**scripts/examples/plan-workflow.cx** - Complete workflow:
```bash
cat scripts/examples/plan-workflow.cx | ./vfsh
```
This script walks through:
- Planning a realistic project (e-commerce app)
- Breaking down into subplans
- Completing tasks and tracking progress
- Using all plan node types together

**scripts/examples/plan-sexp-demo.sexp** - S-expression example:
```bash
# View the structured approach
cat scripts/examples/plan-sexp-demo.sexp
```
This shows how AI can programmatically create and manage plans.

### Planner Documentation

- **scripts/tutorial/planner-tutorial.md** - Complete step-by-step tutorial
- **scripts/reference/plan-commands.md** - Full command reference with examples

### Quick Planner Workflow

```bash
# Start with a clean plan
rm -f plan.vfs

# Create your plan
printf 'plan.create /plan/myproject root "My project"\n' | ./vfsh
printf 'plan.create /plan/myproject/jobs jobs\n' | ./vfsh
printf 'plan.jobs.add /plan/myproject/jobs "First task" 10 agent\n' | ./vfsh
printf 'plan.save\nexit\n' | ./vfsh

# Plan is saved - reload it later
./vfsh
> tree /plan
> exit
```

### Planner Command Patterns

**Pattern 1: Project Planning**
```bash
plan.create /plan/myapp root "Application name"
plan.create /plan/myapp/goals goals
plan.create /plan/myapp/strategy strategy "Your approach"
plan.create /plan/myapp/jobs jobs
plan.jobs.add /plan/myapp/jobs "Task description" 10 agent
plan.save
```

**Pattern 2: Hierarchical Breakdown**
```bash
plan.create /plan/project root "Main project"
plan.create /plan/project/backend subplan "Backend work"
plan.create /plan/project/frontend subplan "Frontend work"
plan.create /plan/project/backend/jobs jobs
plan.create /plan/project/frontend/jobs jobs
plan.save
```

**Pattern 3: Context Management for AI**
```bash
plan.goto /plan/myapp
plan.context.add /plan/myapp/goals
plan.context.add /plan/myapp/jobs
plan.forward
plan.status
# Now ready for AI discussion with focused context
```

## See Also

- [README.md](README.md) - Build and setup instructions
- [VfsShell/AGENTS.md](VfsShell/AGENTS.md) - Implementation architecture
- [AGENTS.md](AGENTS.md) - High-level architecture
- [scripts/tutorial/planner-tutorial.md](scripts/tutorial/planner-tutorial.md) - Complete planner tutorial
- [scripts/reference/plan-commands.md](scripts/reference/plan-commands.md) - Planner command reference
- `tests/` directory - Test script examples
- `scripts/examples/` - Working script examples
