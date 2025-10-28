#pragma once

//
// Planner AST nodes (hierarchical planning system)
//
struct PlanNode : AstNode {
    std::string content;  // Text content for the plan node
    std::map<std::string, std::shared_ptr<VfsNode>> ch;

    PlanNode(std::string n, std::string c = "") : AstNode(std::move(n)), content(std::move(c)) {}
    bool isDir() const override { return true; }
    std::map<std::string, std::shared_ptr<VfsNode>>& children() override { return ch; }
    Value eval(std::shared_ptr<Env>) override { return Value::S(content); }
    std::string read() const override { return content; }
    void write(const std::string& s) override { content = s; }
};

// Specific plan node types
struct PlanRoot : PlanNode {
    PlanRoot(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

struct PlanSubPlan : PlanNode {
    PlanSubPlan(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

struct PlanGoals : PlanNode {
    std::vector<std::string> goals;
    PlanGoals(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanIdeas : PlanNode {
    std::vector<std::string> ideas;
    PlanIdeas(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanStrategy : PlanNode {
    PlanStrategy(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

struct PlanJob {
    std::string description;
    int priority;  // Lower number = higher priority
    bool completed;
    std::string assignee;  // "user" or "agent" or specific agent name
};

struct PlanJobs : PlanNode {
    std::vector<PlanJob> jobs;
    PlanJobs(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
    void addJob(const std::string& desc, int priority = 100, const std::string& assignee = "");
    void completeJob(size_t index);
    std::vector<size_t> getSortedJobIndices() const;
};

struct PlanDeps : PlanNode {
    std::vector<std::string> dependencies;
    PlanDeps(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanImplemented : PlanNode {
    std::vector<std::string> items;
    PlanImplemented(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanResearch : PlanNode {
    std::vector<std::string> topics;
    PlanResearch(std::string n) : PlanNode(std::move(n)) {}
    std::string read() const override;
    void write(const std::string& s) override;
};

struct PlanNotes : PlanNode {
    PlanNotes(std::string n, std::string c = "") : PlanNode(std::move(n), std::move(c)) {}
};

//
// Planner Context (navigator state)
//
struct PlannerContext {
    std::string current_path;  // Current location in /plan tree
    std::vector<std::string> navigation_history;  // Breadcrumbs for backtracking
    std::set<std::string> visible_nodes;  // Paths of nodes currently "in view" for AI context

    enum class Mode { Forward, Backward };
    Mode mode = Mode::Forward;

    void navigateTo(const std::string& path);
    void forward();   // Move towards leaves (add details)
    void backward();  // Move towards root (revise higher-level plans)
    void addToContext(const std::string& vfs_path);
    void removeFromContext(const std::string& vfs_path);
    void clearContext();
};

//
// Discuss Session (conversation state management)
//
struct DiscussSession {
    std::string session_id;  // Named or random hex session identifier
    std::vector<std::string> conversation_history;  // User + AI messages
    std::string current_plan_path;  // Path to active plan in /plan tree (if any)

    enum class Mode {
        Simple,      // Direct AI queries without planning
        Planning,    // Plan-based discussion with breakdown
        Execution    // Working on pre-planned features
    };
    Mode mode = Mode::Simple;

    bool is_active() const { return !session_id.empty(); }
    void clear();
    void add_message(const std::string& role, const std::string& content);
    std::string generate_session_id();
};
