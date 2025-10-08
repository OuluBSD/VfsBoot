# Planner Shell Commands Reference

Complete reference for all `plan.*` commands in VfsBoot.

## Table of Contents
- [Overview](#overview)
- [Plan Node Types](#plan-node-types)
- [Command Reference](#command-reference)
- [Examples](#examples)

## Overview

The planner system provides a hierarchical planning structure stored in the VFS at `/plan`. Plans are automatically persisted to `plan.vfs` and loaded on startup.

Plans consist of various node types that help organize:
- Project goals and ideas
- Execution strategy
- Job tracking with priorities
- Dependencies and research topics
- Implementation history and notes

## Plan Node Types

### PlanRoot
**Purpose**: Top-level plan container
**Fields**: `content` (description text)
**Usage**: Main entry point for a project plan

```
plan.create /plan/myproject root "Build a todo app"
```

### PlanSubPlan
**Purpose**: Nested sub-plan for breaking down complex plans
**Fields**: `content` (description text)
**Usage**: Hierarchical plan decomposition

```
plan.create /plan/myproject/backend subplan "Backend implementation"
```

### PlanGoals
**Purpose**: List of project goals
**Fields**: `goals[]` (list of goal strings)
**Format**: Each line is `goal-N: description`
**Usage**: Define what you want to achieve

```
plan.create /plan/myproject/goals goals
echo /plan/myproject/goals goal-1: Fast performance
echo /plan/myproject/goals goal-2: Easy to use
```

### PlanIdeas
**Purpose**: Brainstorming and idea collection
**Fields**: `ideas[]` (list of idea strings)
**Format**: Each line is `idea-N: description`
**Usage**: Capture potential approaches

```
plan.create /plan/myproject/ideas ideas
echo /plan/myproject/ideas idea-1: Use WebSockets for real-time updates
```

### PlanStrategy
**Purpose**: Overall approach and methodology
**Fields**: `content` (strategy description)
**Usage**: Document high-level approach

```
plan.create /plan/myproject/strategy strategy "Agile development with 2-week sprints"
```

### PlanJobs
**Purpose**: Task tracking with priorities and assignees
**Fields**: `jobs[]` with `{description, priority, completed, assignee}`
**Format**: `[✓] P{priority} {description} (@{assignee})`
**Usage**: Track actionable tasks

```
plan.create /plan/myproject/jobs jobs
plan.jobs.add /plan/myproject/jobs "Implement login" 10 agent
plan.jobs.add /plan/myproject/jobs "Review code" 20 user
```

Priorities: Lower number = higher priority (10 is higher priority than 20)
Assignees: "agent" (AI), "user" (human), or custom names

### PlanDeps
**Purpose**: External dependencies and requirements
**Fields**: `dependencies[]` (list of dependency strings)
**Format**: Each line is `dep-N: description`
**Usage**: Track what's needed

```
plan.create /plan/myproject/deps deps
echo /plan/myproject/deps dep-1: Python 3.11+
echo /plan/myproject/deps dep-2: PostgreSQL database
```

### PlanImplemented
**Purpose**: Completed implementation tracking
**Fields**: `items[]` (list of completed items)
**Format**: Each line is `impl-N: description`
**Usage**: Track what's been built

```
plan.create /plan/myproject/implemented implemented
echo /plan/myproject/implemented impl-1: User authentication system
```

### PlanResearch
**Purpose**: Research topics and investigation areas
**Fields**: `topics[]` (list of research topics)
**Format**: Each line is `topic-N: description`
**Usage**: Track what needs investigation

```
plan.create /plan/myproject/research research
echo /plan/myproject/research topic-1: Best practices for API versioning
```

### PlanNotes
**Purpose**: Free-form notes and reminders
**Fields**: `content` (note text)
**Usage**: General annotations

```
plan.create /plan/myproject/notes notes "Don't forget to update documentation!"
```

---

## Command Reference

### plan.create
**Syntax**: `plan.create <path> <type> [content]`

**Description**: Create a new plan node

**Parameters**:
- `<path>`: VFS path where node will be created (e.g., `/plan/myproject`)
- `<type>`: Node type (root, subplan, goals, ideas, strategy, jobs, deps, implemented, research, notes)
- `[content]`: Optional initial content for text-based nodes (root, subplan, strategy, notes)

**Examples**:
```bash
plan.create /plan/webapp root "E-commerce application"
plan.create /plan/webapp/backend subplan "Backend services"
plan.create /plan/webapp/goals goals
plan.create /plan/webapp/jobs jobs
```

---

### plan.goto
**Syntax**: `plan.goto <path>`

**Description**: Navigate planner context to a specific location

**Parameters**:
- `<path>`: VFS path to navigate to

**Effects**:
- Updates `planner.current_path`
- Adds path to navigation history

**Examples**:
```bash
plan.goto /plan/webapp
plan.goto /plan/webapp/backend/api
```

---

### plan.forward
**Syntax**: `plan.forward`

**Description**: Switch planner to forward mode (adding details)

**Effects**:
- Sets `planner.mode = Forward`
- Indicates intent to elaborate on plans (move toward leaves)

**Use Case**: When you want to add implementation details to existing high-level plans

**Example**:
```bash
plan.forward
# Now AI knows you want to add more specific details
```

---

### plan.backward
**Syntax**: `plan.backward`

**Description**: Switch planner to backward mode (revising strategy)

**Effects**:
- Sets `planner.mode = Backward`
- Indicates intent to revise high-level plans (move toward root)

**Use Case**: When implementation details reveal issues with the overall strategy

**Example**:
```bash
plan.backward
# Now AI knows you want to revise the high-level approach
```

---

### plan.context.add
**Syntax**: `plan.context.add <vfs-path> [vfs-path...]`

**Description**: Add one or more VFS paths to the planner's visible context

**Parameters**:
- `<vfs-path>`: One or more paths to add to context

**Effects**:
- Adds paths to `planner.visible_nodes`
- These nodes will be considered "in view" for AI context building

**Examples**:
```bash
plan.context.add /plan/webapp/goals
plan.context.add /plan/webapp/jobs /plan/webapp/deps
```

---

### plan.context.remove
**Syntax**: `plan.context.remove <vfs-path> [vfs-path...]`

**Description**: Remove one or more VFS paths from the planner context

**Parameters**:
- `<vfs-path>`: One or more paths to remove from context

**Effects**:
- Removes paths from `planner.visible_nodes`

**Examples**:
```bash
plan.context.remove /plan/webapp/goals
plan.context.remove /plan/webapp/jobs /plan/webapp/deps
```

---

### plan.context.clear
**Syntax**: `plan.context.clear`

**Description**: Remove all paths from the planner context

**Effects**:
- Clears `planner.visible_nodes`

**Example**:
```bash
plan.context.clear
```

---

### plan.context.list
**Syntax**: `plan.context.list`

**Description**: Display all paths currently in the planner context

**Output**: Lists all paths in `planner.visible_nodes`

**Example**:
```bash
plan.context.list
# Output:
# planner context (2 paths):
#   /plan/webapp/goals
#   /plan/webapp/jobs
```

---

### plan.status
**Syntax**: `plan.status`

**Description**: Show current planner state

**Output**:
- Current location (`current_path`)
- Navigation mode (`forward` or `backward`)
- Context size (number of visible nodes)
- History depth (navigation breadcrumbs)

**Example**:
```bash
plan.status
# Output:
# planner status:
#   current: /plan/webapp
#   mode: forward
#   context size: 2
#   history depth: 3
```

---

### plan.jobs.add
**Syntax**: `plan.jobs.add <jobs-path> <description> [priority] [assignee]`

**Description**: Add a job to a PlanJobs node

**Parameters**:
- `<jobs-path>`: Path to PlanJobs node
- `<description>`: Job description
- `[priority]`: Optional priority (default: 100, lower = higher priority)
- `[assignee]`: Optional assignee (default: "", common: "agent", "user")

**Examples**:
```bash
plan.jobs.add /plan/webapp/jobs "Setup database" 10 agent
plan.jobs.add /plan/webapp/jobs "Review design" 50 user
plan.jobs.add /plan/webapp/jobs "Write tests" 30
```

---

### plan.jobs.complete
**Syntax**: `plan.jobs.complete <jobs-path> <index>`

**Description**: Mark a job as completed

**Parameters**:
- `<jobs-path>`: Path to PlanJobs node
- `<index>`: Zero-based index of job to complete

**Effects**:
- Sets `job.completed = true`
- Job displays with [✓] checkbox when listed

**Examples**:
```bash
plan.jobs.complete /plan/webapp/jobs 0  # Complete first job
plan.jobs.complete /plan/webapp/jobs 2  # Complete third job
```

**Note**: Jobs are displayed sorted by priority, but indexed in insertion order.

---

### plan.save
**Syntax**: `plan.save [file]`

**Description**: Save the entire `/plan` tree to a VFS overlay file

**Parameters**:
- `[file]`: Optional file path (default: `plan.vfs`)

**Effects**:
- Creates/overwrites the plan file
- Serializes all PlanNode types with full data
- Creates timestamped backup if file already exists

**Examples**:
```bash
plan.save                    # Save to plan.vfs
plan.save myproject.vfs      # Save to custom file
```

**Auto-load**: If `plan.vfs` exists in the current directory, it's automatically loaded on startup.

---

## Examples

### Complete Project Planning Example

```bash
# 1. Create main plan
plan.create /plan/api root "REST API service"

# 2. Define goals
plan.create /plan/api/goals goals
echo /plan/api/goals goal-1: Handle 1000 req/sec
echo /plan/api/goals goal-2: 99.9% uptime
echo /plan/api/goals goal-3: Easy to maintain

# 3. Choose strategy
plan.create /plan/api/strategy strategy "Microservices architecture with event-driven communication"

# 4. List dependencies
plan.create /plan/api/deps deps
echo /plan/api/deps dep-1: Kubernetes cluster
echo /plan/api/deps dep-2: RabbitMQ message broker
echo /plan/api/deps dep-3: Redis cache

# 5. Create job list
plan.create /plan/api/jobs jobs
plan.jobs.add /plan/api/jobs "Design API schema" 5 agent
plan.jobs.add /plan/api/jobs "Setup K8s deployment" 10 agent
plan.jobs.add /plan/api/jobs "Implement auth service" 20 agent
plan.jobs.add /plan/api/jobs "Build user service" 30 agent
plan.jobs.add /plan/api/jobs "Load testing" 40 user

# 6. Research topics
plan.create /plan/api/research research
echo /plan/api/research topic-1: gRPC vs REST performance
echo /plan/api/research topic-2: Service mesh options

# 7. Add notes
plan.create /plan/api/notes notes "Consider using GraphQL for client flexibility"

# 8. Navigate and manage context
plan.goto /plan/api
plan.context.add /plan/api/goals /plan/api/jobs
plan.forward

# 9. Complete a job
plan.jobs.complete /plan/api/jobs 0

# 10. Track implementation
plan.create /plan/api/implemented implemented
echo /plan/api/implemented impl-1: API schema defined in OpenAPI 3.0

# 11. Save
plan.save
```

### Hierarchical Planning Example

```bash
# Create nested structure
plan.create /plan/project root "Full-stack application"
plan.create /plan/project/frontend subplan "React frontend"
plan.create /plan/project/backend subplan "Node.js backend"
plan.create /plan/project/devops subplan "Infrastructure"

# Each subplan gets its own jobs
plan.create /plan/project/frontend/jobs jobs
plan.jobs.add /plan/project/frontend/jobs "Setup React app" 10 agent
plan.jobs.add /plan/project/frontend/jobs "Create components" 20 agent

plan.create /plan/project/backend/jobs jobs
plan.jobs.add /plan/project/backend/jobs "Setup Express server" 10 agent
plan.jobs.add /plan/project/backend/jobs "Design database" 20 agent

plan.create /plan/project/devops/jobs jobs
plan.jobs.add /plan/project/devops/jobs "Configure CI/CD" 10 user
plan.jobs.add /plan/project/devops/jobs "Setup monitoring" 20 agent

# Navigate between subplans
plan.goto /plan/project/frontend
plan.forward

plan.goto /plan/project/backend
plan.status

# Save entire hierarchy
plan.save
```

### Context Management for AI Example

```bash
# Add relevant nodes to AI context for focused discussion
plan.context.add /plan/api/goals
plan.context.add /plan/api/strategy
plan.context.add /plan/api/jobs

# Check what's in context
plan.context.list

# AI call would now see these specific nodes
# ai "Help me prioritize the jobs based on our goals and strategy"

# Change context for different discussion
plan.context.clear
plan.context.add /plan/api/research
plan.context.add /plan/api/deps

# ai "Which research topics should we tackle first given our dependencies?"
```

---

## Best Practices

1. **Start at the top**: Create a root plan first, then break it down
2. **Use subplans**: Keep plans manageable by breaking complex tasks into subplans
3. **Prioritize jobs**: Use priority 1-10 for critical tasks, 11-50 for normal, 51-100 for nice-to-have
4. **Assign tasks**: Use "agent" for AI-suitable tasks, "user" for human-required tasks
5. **Save often**: Run `plan.save` after significant changes
6. **Context management**: Only add relevant nodes to context for focused AI discussions
7. **Track progress**: Update `implemented` list and complete jobs as you progress
8. **Document dependencies**: Keep `deps` updated to track external requirements
9. **Use notes**: Add important reminders and context for future reference
10. **Navigation**: Use `plan.goto` to focus on specific areas before AI discussions

---

## File Format

Plans are saved in VFS overlay format (version 3) with BLAKE3 hashing:

```
# codex-vfs-overlay 3
D /plan
A /plan/myproject PlanRoot 21
  My project description
A /plan/myproject/jobs PlanJobs 60
  <binary job data>
```

Each PlanNode type has custom serialization preserving all fields.

---

## Integration with AI

The planner system is designed to work with AI for:
- **Context filtering**: Use tags and visible_nodes to control what AI sees
- **Forward mode**: AI adds implementation details to high-level plans
- **Backward mode**: AI revises strategy when implementation hits roadblocks
- **Job prioritization**: AI helps prioritize jobs based on goals and dependencies
- **Research guidance**: AI investigates topics and updates research nodes

Future commands (planned):
- `plan.discuss` - Interactive AI planning session
- `plan.hypothesis` - Generate and test planning hypotheses
- `plan.question` - Ask human yes/no/explain questions
