# VfsBoot Planner Tutorial

Learn how to use the VfsBoot planner system for hierarchical project planning and AI-assisted development.

## What You'll Learn

- Creating and organizing project plans
- Managing tasks with priorities and assignments
- Using context management for AI discussions
- Saving and loading plans
- Best practices for effective planning

## Prerequisites

- VfsBoot built and working (`make` in project root)
- Basic familiarity with shell commands

## Tutorial Overview

This tutorial walks through planning a real project: building a blog application.

---

## Part 1: Your First Plan

Let's start by creating a simple project plan.

### Step 1: Start codex

```bash
./codex
```

You should see:
```
codex-mini 🌲 VFS+AST+AI — 'help' kertoo karun totuuden.
>
```

### Step 2: Create a root plan

```bash
plan.create /plan/blog root "Personal blog application"
```

This creates the main container for your project plan.

### Step 3: Check what you created

```bash
tree /plan
```

Output:
```
d plan [base*]
  a blog [base*]
```

The 'a' indicates it's an AST node (PlanRoot is a specialized AST node).

### Step 4: Read your plan

```bash
cat /plan/blog
```

Output:
```
Personal blog application
```

**What you learned**: Plans live in the VFS at `/plan` and can be inspected with standard commands like `tree` and `cat`.

---

## Part 2: Adding Structure

Now let's add goals, strategy, and tasks to your plan.

### Step 5: Define goals

```bash
plan.create /plan/blog/goals goals
echo goal-1: Support markdown posts >> /plan/blog/goals
echo goal-2: Fast page loads >> /plan/blog/goals
echo goal-3: SEO friendly >> /plan/blog/goals
echo goal-4: Easy to maintain >> /plan/blog/goals
```

Check your goals:
```bash
cat /plan/blog/goals
```

Output:
```
goal-1: Support markdown posts
goal-2: Fast page loads
goal-3: SEO friendly
goal-4: Easy to maintain
```

### Step 6: Define your strategy

```bash
plan.create /plan/blog/strategy strategy "Use static site generation with Next.js for performance and SEO"
```

Check it:
```bash
cat /plan/blog/strategy
```

### Step 7: List dependencies

```bash
plan.create /plan/blog/deps deps
echo dep-1: Node.js v18 or higher >> /plan/blog/deps
echo dep-2: Next.js framework >> /plan/blog/deps
echo dep-3: Markdown parser library >> /plan/blog/deps
echo dep-4: Hosting platform (Vercel or Netlify) >> /plan/blog/deps
```

### Step 8: View the plan structure

```bash
tree /plan/blog
```

Output:
```
a blog [base*]
  a deps [base*]
  a goals [base*]
  a strategy [base*]
```

**What you learned**: Different plan node types serve different purposes - goals for objectives, strategy for approach, deps for requirements.

---

## Part 3: Task Management

The most important part: tracking what needs to be done.

### Step 9: Create a job list

```bash
plan.create /plan/blog/jobs jobs
```

### Step 10: Add tasks with priorities

```bash
plan.jobs.add /plan/blog/jobs "Setup Next.js project" 5 agent
plan.jobs.add /plan/blog/jobs "Design page layouts" 15 user
plan.jobs.add /plan/blog/jobs "Implement markdown rendering" 20 agent
plan.jobs.add /plan/blog/jobs "Create blog post listing page" 30 agent
plan.jobs.add /plan/blog/jobs "Add RSS feed" 50 agent
plan.jobs.add /plan/blog/jobs "Setup analytics" 60 user
```

**Priority guide**:
- 1-10: Critical, do first
- 11-30: Important, do soon
- 31-60: Normal priority
- 61-100: Nice to have

**Assignee guide**:
- `agent`: Task suitable for AI assistance
- `user`: Requires human judgment/creativity
- (empty): Not yet assigned

### Step 11: View your jobs

```bash
cat /plan/blog/jobs
```

Output:
```
[ ] P5 Setup Next.js project (@agent)
[ ] P15 Design page layouts (@user)
[ ] P20 Implement markdown rendering (@agent)
[ ] P30 Create blog post listing page (@agent)
[ ] P50 Add RSS feed (@agent)
[ ] P60 Setup analytics (@user)
```

Jobs are automatically sorted by priority!

### Step 12: Complete a job

Let's say you've set up the Next.js project:

```bash
plan.jobs.complete /plan/blog/jobs 0
```

Note: Index 0 is the *first job added*, not necessarily the first in the sorted list.

View updated jobs:
```bash
cat /plan/blog/jobs
```

Output:
```
[✓] P5 Setup Next.js project (@agent)
[ ] P15 Design page layouts (@user)
...
```

**What you learned**: Jobs have priorities and assignees, and can be marked complete.

---

## Part 4: Hierarchical Planning

For complex projects, break plans into subplans.

### Step 13: Create subplans

```bash
plan.create /plan/blog/content subplan "Content management"
plan.create /plan/blog/frontend subplan "User interface"
plan.create /plan/blog/deploy subplan "Deployment and hosting"
```

### Step 14: Add jobs to subplans

```bash
# Content management jobs
plan.create /plan/blog/content/jobs jobs
plan.jobs.add /plan/blog/content/jobs "Create markdown parser" 10 agent
plan.jobs.add /plan/blog/content/jobs "Add syntax highlighting" 20 agent
plan.jobs.add /plan/blog/content/jobs "Support frontmatter metadata" 15 agent

# Frontend jobs
plan.create /plan/blog/frontend/jobs jobs
plan.jobs.add /plan/blog/frontend/jobs "Design homepage" 10 user
plan.jobs.add /plan/blog/frontend/jobs "Build post template" 20 agent
plan.jobs.add /plan/blog/frontend/jobs "Add dark mode" 50 agent

# Deployment jobs
plan.create /plan/blog/deploy/jobs jobs
plan.jobs.add /plan/blog/deploy/jobs "Configure Vercel project" 10 user
plan.jobs.add /plan/blog/deploy/jobs "Setup custom domain" 20 user
plan.jobs.add /plan/blog/deploy/jobs "Add build optimization" 30 agent
```

### Step 15: View the hierarchy

```bash
tree /plan/blog
```

Output:
```
a blog [base*]
  a content [base*]
    a jobs [base*]
  a deploy [base*]
    a jobs [base*]
  a deps [base*]
  a frontend [base*]
    a jobs [base*]
  a goals [base*]
  a jobs [base*]
  a strategy [base*]
```

**What you learned**: Subplans help organize complex projects into manageable pieces.

---

## Part 5: Navigation and Context

Navigate through your plan and manage AI context.

### Step 16: Navigate to a subplan

```bash
plan.goto /plan/blog/content
plan.status
```

Output:
```
planner status:
  current: /plan/blog/content
  mode: forward
  context size: 0
  history depth: 1
```

### Step 17: Understanding navigation modes

**Forward mode** (default): Adding implementation details

```bash
plan.forward
```

Use this when you want to elaborate on existing plans, add more specific tasks, or drill down into details.

**Backward mode**: Revising high-level strategy

```bash
plan.backward
```

Use this when implementation reveals problems with the overall approach and you need to rethink the strategy.

### Step 18: Managing AI context

Add specific nodes to the AI's "view":

```bash
plan.context.add /plan/blog/goals
plan.context.add /plan/blog/content/jobs
plan.context.list
```

Output:
```
planner context (2 paths):
  /plan/blog/goals
  /plan/blog/content/jobs
```

When you call the AI (future `plan.discuss` command), it will focus on these nodes.

Remove from context:
```bash
plan.context.remove /plan/blog/goals
```

Clear all:
```bash
plan.context.clear
```

**What you learned**: Navigation tracks where you are in the plan, and context management controls what the AI sees.

---

## Part 6: Research and Notes

Track what needs investigation and important reminders.

### Step 19: Add research topics

```bash
plan.create /plan/blog/research research
echo topic-1: Best markdown-to-HTML libraries >> /plan/blog/research
echo topic-2: Static site vs SSR tradeoffs >> /plan/blog/research
echo topic-3: Image optimization techniques >> /plan/blog/research
```

### Step 20: Add notes

```bash
plan.create /plan/blog/notes notes "Remember to add social media meta tags for sharing. Consider adding a newsletter signup form in the future."
```

### Step 21: Track what's been implemented

```bash
plan.create /plan/blog/implemented implemented
echo impl-1: Next.js project initialized with TypeScript >> /plan/blog/implemented
echo impl-2: Basic page routing structure created >> /plan/blog/implemented
```

**What you learned**: Research tracks open questions, notes hold reminders, implemented tracks progress.

---

## Part 7: Saving and Loading

Persist your plan to disk.

### Step 22: Save your plan

```bash
plan.save
```

Output:
```
saved plan tree to /path/to/plan.vfs
```

This creates `plan.vfs` in your current directory.

### Step 23: Exit and reload

```bash
exit
```

Start codex again:
```bash
./codex
```

Output shows:
```
auto-loaded plan.vfs into /plan tree
codex-mini 🌲 VFS+AST+AI — 'help' kertoo karun totuuden.
```

Your plan is back! Check it:
```bash
tree /plan/blog
```

**What you learned**: Plans automatically save/load from `plan.vfs`, preserving all structure and data.

---

## Part 8: Practical Workflow

Let's walk through a realistic workflow.

### Step 24: Morning planning session

Start your day by reviewing jobs:

```bash
# Navigate to current focus area
plan.goto /plan/blog/content

# Check what needs doing
cat /plan/blog/content/jobs

# Add context for AI discussion
plan.context.add /plan/blog/content/jobs
plan.context.add /plan/blog/goals
plan.forward
```

### Step 25: Working on tasks

As you complete tasks:

```bash
# Complete the markdown parser task
plan.jobs.complete /plan/blog/content/jobs 0

# Add what you implemented
echo impl-3: Markdown parser with code highlighting >> /plan/blog/implemented

# Save progress
plan.save
```

### Step 26: Discovering new work

As you work, you discover more tasks:

```bash
plan.jobs.add /plan/blog/content/jobs "Handle image paths in markdown" 25 agent
plan.jobs.add /plan/blog/content/jobs "Add table of contents generation" 40 agent
```

### Step 27: Research and strategy updates

You researched a topic:

```bash
# Update research with findings
cat /plan/blog/research
# ... shows topic-2: Static site vs SSR tradeoffs

# Add findings to notes
echo >> /plan/blog/notes
echo "Research finding: Static generation is best for this use case. Pages rarely change and SEO is critical." >> /plan/blog/notes
```

Strategy needs updating:

```bash
# Update strategy
echo "Updated: Using full static generation (SSG) with incremental static regeneration (ISR) for new posts" > /tmp/strategy.txt
cat /tmp/strategy.txt > /plan/blog/strategy
```

### Step 28: End of day

```bash
# Review what was accomplished
cat /plan/blog/implemented

# Check remaining jobs
cat /plan/blog/content/jobs

# Save everything
plan.save
```

**What you learned**: The planner supports an iterative workflow - plan, execute, discover, update, repeat.

---

## Part 9: Advanced Patterns

### Pattern 1: Multi-phase projects

```bash
plan.create /plan/blog/phase1 subplan "MVP - Basic blog"
plan.create /plan/blog/phase2 subplan "Enhanced features"
plan.create /plan/blog/phase3 subplan "Community features"

# Each phase has its own jobs
plan.create /plan/blog/phase1/jobs jobs
plan.jobs.add /plan/blog/phase1/jobs "Core blogging features" 10 agent

plan.create /plan/blog/phase2/jobs jobs
plan.jobs.add /plan/blog/phase2/jobs "Comments system" 10 agent

plan.create /plan/blog/phase3/jobs jobs
plan.jobs.add /plan/blog/phase3/jobs "User submissions" 10 agent
```

### Pattern 2: Team collaboration

```bash
plan.create /plan/blog/team subplan "Team assignments"
plan.create /plan/blog/team/alice jobs
plan.jobs.add /plan/blog/team/alice "Design color scheme" 10 alice
plan.jobs.add /plan/blog/team/alice "Create logo" 20 alice

plan.create /plan/blog/team/bob jobs
plan.jobs.add /plan/blog/team/bob "Setup CI/CD pipeline" 10 bob
plan.jobs.add /plan/blog/team/bob "Configure monitoring" 20 bob
```

### Pattern 3: Risk tracking

```bash
plan.create /plan/blog/risks research
echo risk-1: Hosting costs may exceed budget at scale >> /plan/blog/risks
echo risk-2: Markdown parser security vulnerabilities >> /plan/blog/risks
echo risk-3: SEO effectiveness uncertain >> /plan/blog/risks
```

**What you learned**: The planner is flexible - adapt it to your workflow and project needs.

---

## Part 10: Integration with AI (Preview)

The planner is designed to work with AI. Here's how it will work (commands coming soon):

### Future workflow example:

```bash
# Set context for AI discussion
plan.goto /plan/blog/content
plan.context.add /plan/blog/goals
plan.context.add /plan/blog/content/jobs
plan.forward

# Start AI discussion (future command)
# plan.discuss "Help me implement the markdown parser"

# AI sees:
# - Current location: /plan/blog/content
# - Mode: forward (wants details)
# - Context: goals and content jobs
# - Can suggest: specific implementation steps, libraries, code structure

# AI might ask questions:
# - "Should syntax highlighting support all languages or just popular ones?"
# - "Do you want to support GitHub-flavored markdown extensions?"

# AI can:
# - Add new jobs it discovers
# - Update research topics
# - Suggest priority changes
# - Generate code based on the plan
```

**What to expect**: AI will use the plan structure to provide focused, relevant assistance that aligns with your goals and strategy.

---

## Summary

You've learned:

✓ Creating plans with root nodes and subplans
✓ Using different node types (goals, strategy, jobs, deps, research, notes, implemented)
✓ Managing tasks with priorities and assignees
✓ Completing jobs and tracking progress
✓ Navigating hierarchical plans
✓ Using context management for focused work
✓ Saving and loading plans
✓ Practical daily workflow patterns
✓ Advanced organizational patterns

## Next Steps

1. **Practice**: Create a plan for one of your own projects
2. **Experiment**: Try different structures and see what works for you
3. **Scripts**: Write .cx scripts to automate your planning workflow
4. **Explore**: Check out the example scripts in `scripts/examples/`
5. **Reference**: Keep `scripts/reference/plan-commands.md` handy

## Common Pitfalls

❌ **Don't** create plans without saving them
✓ **Do** run `plan.save` regularly

❌ **Don't** make job descriptions too vague ("Fix stuff")
✓ **Do** be specific ("Fix login validation for edge case where email contains +")

❌ **Don't** ignore priorities
✓ **Do** use priorities to guide your work order

❌ **Don't** nest too deeply (more than 3-4 levels)
✓ **Do** keep hierarchy manageable

❌ **Don't** forget to mark jobs complete
✓ **Do** track progress with `plan.jobs.complete`

## Getting Help

- Run `help` in codex to see all commands
- Check `scripts/reference/plan-commands.md` for detailed reference
- Look at `scripts/examples/plan-*.cx` for more examples
- Read AGENTS.md for technical details

---

Happy planning! 🎯
