#pragma once


// ============================================================================
// Feedback Pipeline for Planner Rule Evolution
// ============================================================================

// Execution metrics captured during planner runs
struct PlannerMetrics {
    std::string scenario_name;
    uint64_t timestamp;
    bool success;
    size_t iterations;
    size_t rules_applied;
    std::vector<std::string> rules_triggered;  // Names of rules that fired
    std::vector<std::string> rules_failed;     // Names of rules that were expected but didn't fire
    std::string error_message;

    // Performance metrics
    double execution_time_ms;
    size_t context_tokens_used;
    size_t vfs_nodes_examined;

    // Outcome metrics
    bool plan_matched_expected;
    bool actions_completed;
    bool verification_passed;

    PlannerMetrics()
        : timestamp(0), success(false), iterations(0), rules_applied(0),
          execution_time_ms(0.0), context_tokens_used(0), vfs_nodes_examined(0),
          plan_matched_expected(false), actions_completed(false), verification_passed(false) {}
};

// Metrics collector for capturing execution data
struct MetricsCollector {
    std::vector<PlannerMetrics> history;
    PlannerMetrics* current_metrics;

    MetricsCollector() : current_metrics(nullptr) {}

    // Start collecting metrics for a new run
    void startRun(const std::string& scenario_name);

    // Record rule trigger
    void recordRuleTrigger(const std::string& rule_name);
    void recordRuleFailed(const std::string& rule_name);

    // Record outcome
    void recordSuccess(bool success, const std::string& error = "");
    void recordIterations(size_t count);
    void recordPerformance(double exec_time_ms, size_t tokens, size_t nodes);
    void recordOutcome(bool plan_matched, bool actions_completed, bool verification_passed);

    // Finish current run and add to history
    void finishRun();

    // Analysis
    std::vector<std::string> getMostTriggeredRules(size_t top_n = 10) const;
    std::vector<std::string> getMostFailedRules(size_t top_n = 10) const;
    double getAverageSuccessRate() const;
    double getAverageIterations() const;

    // Persistence
    void saveToVfs(Vfs& vfs, const std::string& path = "/plan/metrics") const;
    void loadFromVfs(Vfs& vfs, const std::string& path = "/plan/metrics");
};

// Rule patch represents a proposed modification to an implication rule
struct RulePatch {
    enum class Operation {
        Add,            // Add new rule
        Modify,         // Modify existing rule (change confidence/formulas)
        Remove,         // Remove rule
        AdjustConfidence // Only adjust confidence value
    };

    Operation operation;
    std::string rule_name;

    // For Add/Modify operations
    std::shared_ptr<LogicFormula> new_premise;
    std::shared_ptr<LogicFormula> new_conclusion;
    float new_confidence;
    std::string source;  // "ai-generated", "learned", "user"

    // Rationale for the patch
    std::string rationale;

    // Supporting evidence from metrics
    std::vector<std::string> supporting_scenarios;
    size_t evidence_count;

    RulePatch()
        : operation(Operation::Add), new_confidence(1.0f), evidence_count(0) {}

    // Create patches
    static RulePatch addRule(const std::string& name,
                             std::shared_ptr<LogicFormula> premise,
                             std::shared_ptr<LogicFormula> conclusion,
                             float confidence,
                             const std::string& source,
                             const std::string& rationale);

    static RulePatch modifyRule(const std::string& name,
                                std::shared_ptr<LogicFormula> new_premise,
                                std::shared_ptr<LogicFormula> new_conclusion,
                                float new_confidence,
                                const std::string& rationale);

    static RulePatch removeRule(const std::string& name, const std::string& rationale);

    static RulePatch adjustConfidence(const std::string& name,
                                      float new_confidence,
                                      const std::string& rationale);

    // Serialization
    std::string serialize(const TagRegistry& reg) const;
    static RulePatch deserialize(const std::string& data, TagRegistry& reg);
};

// Rule patch staging area for managing proposed changes
struct RulePatchStaging {
    std::vector<RulePatch> pending_patches;
    std::vector<RulePatch> applied_patches;
    std::vector<RulePatch> rejected_patches;

    LogicEngine* logic_engine;

    explicit RulePatchStaging(LogicEngine* engine) : logic_engine(engine) {}

    // Add patch to staging
    void stagePatch(const RulePatch& patch);
    void stagePatch(RulePatch&& patch);

    // Review patches
    size_t pendingCount() const { return pending_patches.size(); }
    const RulePatch& getPatch(size_t index) const { return pending_patches.at(index); }
    std::vector<RulePatch> listPending() const { return pending_patches; }

    // Apply or reject patches
    bool applyPatch(size_t index);
    bool rejectPatch(size_t index, const std::string& reason = "");
    bool applyAll();
    void rejectAll();

    // Clear staging area
    void clearPending();
    void clearApplied();
    void clearRejected();
    void clearAll();

    // Persistence
    void savePendingToVfs(Vfs& vfs, const std::string& path = "/plan/patches/pending") const;
    void saveAppliedToVfs(Vfs& vfs, const std::string& path = "/plan/patches/applied") const;
    void loadPendingFromVfs(Vfs& vfs, const std::string& path = "/plan/patches/pending");
};

// Feedback loop orchestrator
struct FeedbackLoop {
    MetricsCollector& metrics_collector;
    RulePatchStaging& patch_staging;
    LogicEngine& logic_engine;
    TagRegistry& tag_registry;

    // AI integration (optional)
    bool use_ai_assistance;
    std::string ai_provider;  // "openai" or "llama"

    FeedbackLoop(MetricsCollector& mc, RulePatchStaging& ps, LogicEngine& le, TagRegistry& tr)
        : metrics_collector(mc), patch_staging(ps), logic_engine(le), tag_registry(tr),
          use_ai_assistance(false), ai_provider("openai") {}

    // Analyze metrics and generate rule patches
    std::vector<RulePatch> analyzeMetrics(size_t min_evidence_count = 3);

    // Generate patches from failure patterns
    std::vector<RulePatch> generatePatchesFromFailures();

    // Generate patches from success patterns
    std::vector<RulePatch> generatePatchesFromSuccesses();

    // Generate patches with AI assistance
    std::vector<RulePatch> generatePatchesWithAI(const std::string& context_prompt);

    // Run full feedback cycle
    struct FeedbackCycleResult {
        size_t patches_generated;
        size_t patches_staged;
        size_t patches_applied;
        std::string summary;
    };

    FeedbackCycleResult runCycle(bool auto_apply = false, size_t min_evidence = 3);

    // Interactive review mode
    void interactiveReview();

private:
    // Pattern detection helpers
    struct RulePattern {
        std::string rule_name;
        size_t trigger_count;
        size_t success_count;
        size_t failure_count;
        double success_rate;
    };

    std::vector<RulePattern> detectPatterns() const;
    bool shouldIncreaseConfidence(const RulePattern& pattern) const;
    bool shouldDecreaseConfidence(const RulePattern& pattern) const;
    bool shouldRemoveRule(const RulePattern& pattern) const;
};


