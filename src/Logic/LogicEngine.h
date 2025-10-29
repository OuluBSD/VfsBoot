#ifndef _Logic_LogicEngine_h_
#define _Logic_LogicEngine_h_

// All includes have been moved to Logic.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs types from VfsCore.h and TagSystem.h
// #include "../VfsCore/VfsCore.h"  // Commented for U++ convention - included in main header
// #include "../Logic/TagSystem.h"  // Commented for U++ convention - included in main header

// Forward declarations for types in other packages
struct TagRegistry;
struct Vfs;

//
// Logic system for tag theorem proving
//
enum class LogicOp { VAR, NOT, AND, OR, IMPLIES };

struct LogicFormula {
    LogicOp op;
    TagId var_id;  // for VAR
    Vector<One<LogicFormula>> children;

    static One<LogicFormula> makeVar(TagId id);
    static One<LogicFormula> makeNot(One<LogicFormula> f);
    static One<LogicFormula> makeAnd(Vector<One<LogicFormula>> fs);
    static One<LogicFormula> makeOr(Vector<One<LogicFormula>> fs);
    static One<LogicFormula> makeImplies(One<LogicFormula> lhs, One<LogicFormula> rhs);

    bool evaluate(const TagSet& tags) const;
    String toString(const TagRegistry& reg) const;
    typedef LogicFormula CLASSNAME;  // Required for THISBACK macros if used
};

struct ImplicationRule {
    String name;
    One<LogicFormula> premise;
    One<LogicFormula> conclusion;
    float confidence;  // 0.0 to 1.0, 1.0 = always true
    String source;  // "hardcoded", "learned", "ai-generated", "user"

    ImplicationRule(String n, One<LogicFormula> p, One<LogicFormula> c,
                   float conf = 1.0f, String src = "hardcoded")
        : name(pick(n)), premise(pick(p)), conclusion(pick(c)), confidence(conf), source(pick(src)) {}
};

struct LogicEngine {
    Vector<ImplicationRule> rules;
    TagRegistry* tag_registry;

    explicit LogicEngine(TagRegistry* reg) : tag_registry(reg) {}

    // Add rules
    void addRule(const ImplicationRule& rule);
    void addHardcodedRules();  // built-in domain knowledge

    // Forward chaining: infer all implied tags from given tags
    TagSet inferTags(const TagSet& initial_tags, float min_confidence = 0.8f) const;

    // Consistency checking
    struct ConflictInfo {
        String description;
        Vector<String> conflicting_tags;
        Vector<String> suggestions;
    };
    std::optional<ConflictInfo> checkConsistency(const TagSet& tags) const;

    // Find contradictions in formula (SAT solving)
    bool isSatisfiable(One<LogicFormula> formula) const;

    // Explain why tags are inferred
    Vector<String> explainInference(TagId tag, const TagSet& initial_tags) const;

    // Rule persistence
    void saveRulesToVfs(Vfs& vfs, const String& base_path = "/plan/rules") const;
    void loadRulesFromVfs(Vfs& vfs, const String& base_path = "/plan/rules");
    String serializeRule(const ImplicationRule& rule) const;
    ImplicationRule deserializeRule(const String& serialized) const;

    // Dynamic rule creation
    void addSimpleRule(const String& name, const String& premise_tag,
                       const String& conclusion_tag, float confidence, const String& source);
    void addExclusionRule(const String& name, const String& tag1,
                          const String& tag2, const String& source = "user");
    void removeRule(const String& name);
    bool hasRule(const String& name) const;
};

#endif
