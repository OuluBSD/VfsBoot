#pragma once



// Test suite for action planner hypothesis validation
struct ActionPlannerTest {
    std::string name;
    std::string description;
    std::function<bool()> test_fn;
    bool passed;
    std::string error_message;

    ActionPlannerTest(std::string n, std::string desc, std::function<bool()> fn)
        : name(std::move(n)), description(std::move(desc)), test_fn(std::move(fn)), passed(false) {}

    bool run();
};

struct ActionPlannerTestSuite {
    std::vector<ActionPlannerTest> tests;
    Vfs& vfs;
    TagStorage& tag_storage;
    TagRegistry& tag_registry;

    ActionPlannerTestSuite(Vfs& v, TagStorage& ts, TagRegistry& tr)
        : vfs(v), tag_storage(ts), tag_registry(tr) {}

    void addTest(const std::string& name, const std::string& desc, std::function<bool()> fn);
    void runAll();
    void printResults() const;
    size_t passedCount() const;
    size_t failedCount() const;
};

//
// Hypothesis Testing System (5 progressive complexity levels)
//

// Hypothesis represents a testable proposition about code structure or behavior
struct Hypothesis {
    enum class Level {
        SimpleQuery = 1,       // Find functions/patterns in VFS
        CodeModification = 2,  // Add error handling to existing code
        Refactoring = 3,       // Extract duplicated code
        FeatureAddition = 4,   // Add logging/instrumentation
        Architecture = 5       // Implement design patterns
    };

    Level level;
    std::string description;
    std::string goal;
    std::vector<std::string> assumptions;
    std::vector<std::string> validation_criteria;
    bool tested;
    bool valid;
    std::string result;

    Hypothesis(Level lvl, std::string desc, std::string g)
        : level(lvl), description(std::move(desc)), goal(std::move(g)),
          tested(false), valid(false) {}

    void addAssumption(const std::string& assumption);
    void addValidation(const std::string& criterion);
    std::string levelName() const;
};

// Hypothesis test result with detailed diagnostics
struct HypothesisResult {
    bool success;
    std::string message;
    std::vector<std::string> findings;  // What was found
    std::vector<std::string> actions;   // What would be done
    size_t nodes_examined;
    size_t nodes_matched;

    HypothesisResult() : success(false), nodes_examined(0), nodes_matched(0) {}

    void addFinding(const std::string& finding);
    void addAction(const std::string& action);
    std::string summary() const;
};

// Hypothesis tester performs validation without calling AI
struct HypothesisTester {
    Vfs& vfs;
    TagStorage& tag_storage;
    TagRegistry& tag_registry;
    ContextBuilder context_builder;

    HypothesisTester(Vfs& v, TagStorage& ts, TagRegistry& tr)
        : vfs(v), tag_storage(ts), tag_registry(tr),
          context_builder(v, ts, tr, 4000) {}

    // Level 1: Find function/pattern in VFS
    HypothesisResult testSimpleQuery(const std::string& target, const std::string& search_path = "/");

    // Level 2: Validate error handling addition strategy
    HypothesisResult testErrorHandlingAddition(const std::string& function_name,
                                                const std::string& error_handling_style);

    // Level 3: Identify duplicate code blocks for extraction
    HypothesisResult testDuplicateExtraction(const std::string& search_path,
                                              size_t min_similarity_lines = 3);

    // Level 4: Plan logging instrumentation for error paths
    HypothesisResult testLoggingInstrumentation(const std::string& search_path);

    // Level 5: Evaluate architectural pattern applicability
    HypothesisResult testArchitecturePattern(const std::string& pattern_name,
                                              const std::string& target_path);

    // General hypothesis testing interface
    HypothesisResult test(Hypothesis& hypothesis);

private:
    // Helper methods
    std::vector<std::string> findFunctionDefinitions(const std::string& path);
    std::vector<std::string> findReturnPaths(const std::string& function_content);
    std::vector<std::pair<std::string, std::string>> findDuplicateBlocks(
        const std::string& path, size_t min_lines);
    std::vector<std::string> findErrorPaths(const std::string& path);
    bool contentSimilar(const std::string& a, const std::string& b, size_t min_lines);
};

// Hypothesis test suite for all 5 levels
struct HypothesisTestSuite {
    HypothesisTester tester;
    std::vector<Hypothesis> hypotheses;

    HypothesisTestSuite(Vfs& v, TagStorage& ts, TagRegistry& tr)
        : tester(v, ts, tr) {}

    void addHypothesis(Hypothesis h);
    void runAll();
    void printResults() const;
    size_t validCount() const;
    size_t invalidCount() const;
    size_t untestedCount() const;

    // Create standard test suite with examples for all 5 levels
    void createStandardSuite();
};
