#pragma once


//
// Logic system for tag theorem proving
//
enum class LogicOp { VAR, NOT, AND, OR, IMPLIES };

struct LogicFormula {
    LogicOp op;
    TagId var_id;  // for VAR
    std::vector<std::shared_ptr<LogicFormula>> children;

    static std::shared_ptr<LogicFormula> makeVar(TagId id);
    static std::shared_ptr<LogicFormula> makeNot(std::shared_ptr<LogicFormula> f);
    static std::shared_ptr<LogicFormula> makeAnd(std::vector<std::shared_ptr<LogicFormula>> fs);
    static std::shared_ptr<LogicFormula> makeOr(std::vector<std::shared_ptr<LogicFormula>> fs);
    static std::shared_ptr<LogicFormula> makeImplies(std::shared_ptr<LogicFormula> lhs, std::shared_ptr<LogicFormula> rhs);

    bool evaluate(const TagSet& tags) const;
    std::string toString(const TagRegistry& reg) const;
};

struct ImplicationRule {
    std::string name;
    std::shared_ptr<LogicFormula> premise;
    std::shared_ptr<LogicFormula> conclusion;
    float confidence;  // 0.0 to 1.0, 1.0 = always true
    std::string source;  // "hardcoded", "learned", "ai-generated", "user"

    ImplicationRule(std::string n, std::shared_ptr<LogicFormula> p, std::shared_ptr<LogicFormula> c,
                   float conf = 1.0f, std::string src = "hardcoded")
        : name(std::move(n)), premise(p), conclusion(c), confidence(conf), source(std::move(src)) {}
};

struct LogicEngine {
    std::vector<ImplicationRule> rules;
    TagRegistry* tag_registry;

    explicit LogicEngine(TagRegistry* reg) : tag_registry(reg) {}

    // Add rules
    void addRule(const ImplicationRule& rule);
    void addHardcodedRules();  // built-in domain knowledge

    // Forward chaining: infer all implied tags from given tags
    TagSet inferTags(const TagSet& initial_tags, float min_confidence = 0.8f) const;

    // Consistency checking
    struct ConflictInfo {
        std::string description;
        std::vector<std::string> conflicting_tags;
        std::vector<std::string> suggestions;
    };
    std::optional<ConflictInfo> checkConsistency(const TagSet& tags) const;

    // Find contradictions in formula (SAT solving)
    bool isSatisfiable(std::shared_ptr<LogicFormula> formula) const;

    // Explain why tags are inferred
    std::vector<std::string> explainInference(TagId tag, const TagSet& initial_tags) const;

    // Rule persistence
    void saveRulesToVfs(Vfs& vfs, const std::string& base_path = "/plan/rules") const;
    void loadRulesFromVfs(Vfs& vfs, const std::string& base_path = "/plan/rules");
    std::string serializeRule(const ImplicationRule& rule) const;
    ImplicationRule deserializeRule(const std::string& serialized) const;

    // Dynamic rule creation
    void addSimpleRule(const std::string& name, const std::string& premise_tag,
                       const std::string& conclusion_tag, float confidence, const std::string& source);
    void addExclusionRule(const std::string& name, const std::string& tag1,
                          const std::string& tag2, const std::string& source = "user");
    void removeRule(const std::string& name);
    bool hasRule(const std::string& name) const;
};
