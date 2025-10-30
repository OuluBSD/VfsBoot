#include "Logic.h"
#include "../VfsShell/VfsShell.h"

// ====== Logic System for Tag Theorem Proving ======

// LogicFormula factory methods
One<LogicFormula> LogicFormula::makeVar(TagId id){
    auto f = new LogicFormula();
    f->op = LogicOp::VAR;
    f->var_id = id;
    return One<LogicFormula>(f);
}

One<LogicFormula> LogicFormula::makeNot(One<LogicFormula> f){
    auto result = new LogicFormula();
    result->op = LogicOp::NOT;
    result->children.Add(pick(f));
    return One<LogicFormula>(result);
}

One<LogicFormula> LogicFormula::makeAnd(Vector<One<LogicFormula>> fs){
    auto result = new LogicFormula();
    result->op = LogicOp::AND;
    result->children = pick(fs);
    return One<LogicFormula>(result);
}

One<LogicFormula> LogicFormula::makeOr(Vector<One<LogicFormula>> fs){
    auto result = new LogicFormula();
    result->op = LogicOp::OR;
    result->children = pick(fs);
    return One<LogicFormula>(result);
}

One<LogicFormula> LogicFormula::makeImplies(One<LogicFormula> lhs, One<LogicFormula> rhs){
    auto result = new LogicFormula();
    result->op = LogicOp::IMPLIES;
    result->children.Add(pick(lhs));
    result->children.Add(pick(rhs));
    return result;
}

// Evaluate formula given a tag set
bool LogicFormula::evaluate(const TagSet& tags) const {
    switch(op){
        case LogicOp::VAR:
            return tags.count(var_id) > 0;
        case LogicOp::NOT:
            return !children[0]->evaluate(tags);
        case LogicOp::AND:
            for(const auto& child : children){
                if(!child->evaluate(tags)) return false;
            }
            return true;
        case LogicOp::OR:
            for(const auto& child : children){
                if(child->evaluate(tags)) return true;
            }
            return false;
        case LogicOp::IMPLIES:
            // A -> B is equivalent to !A || B
            return !children[0]->evaluate(tags) || children[1]->evaluate(tags);
    }
    return false;
}

// Convert formula to string for debugging
String LogicFormula::toString(const TagRegistry& reg) const {
    switch(op){
        case LogicOp::VAR:
            return reg.getTagName(var_id);
        case LogicOp::NOT:
            return String("(not ") + children[0]->toString(reg) + ")";
        case LogicOp::AND:{
            String result = "(and";
            for(const auto& child : children){
                result += String(" ") + child->toString(reg);
            }
            return result + ")";
        }
        case LogicOp::OR:{
            String result = "(or";
            for(const auto& child : children){
                result += String(" ") + child->toString(reg);
            }
            return result + ")";
        }
        case LogicOp::IMPLIES:
            return String("(implies ") + children[0]->toString(reg) + " " + children[1]->toString(reg) + ")";
    }
    return "?";
}

One<LogicFormula> LogicFormula::clone() const {
    auto result = new LogicFormula();
    result->op = op;
    result->var_id = var_id;
    for(const auto& child : children) {
        result->children.Add(child->clone());
    }
    return One<LogicFormula>(result);
}

// LogicEngine implementation
void LogicEngine::addRule(ImplicationRule&& rule){
    rules.Add(pick(rule));
}

void LogicEngine::addHardcodedRules(){
    // Rule: offline implies not network
    TagId offline_id = tag_registry->registerTag("offline");
    TagId network_id = tag_registry->registerTag("network");
    addRule(ImplicationRule(
        "offline-no-network",
        LogicFormula::makeVar(offline_id),
        LogicFormula::makeNot(LogicFormula::makeVar(network_id)),
        1.0f, "hardcoded"
    ));

    // Rule: fast usually implies cached (high confidence learned pattern)
    TagId fast_id = tag_registry->registerTag("fast");
    TagId cached_id = tag_registry->registerTag("cached");
    addRule(ImplicationRule(
        "fast-cached",
        LogicFormula::makeVar(fast_id),
        LogicFormula::makeVar(cached_id),
        0.87f, "learned"
    ));

    // Rule: cached implies not remote
    TagId remote_id = tag_registry->registerTag("remote");
    addRule(ImplicationRule(
        "cached-not-remote",
        LogicFormula::makeVar(cached_id),
        LogicFormula::makeNot(LogicFormula::makeVar(remote_id)),
        1.0f, "hardcoded"
    ));

    // Rule: no-network implies offline
    TagId no_network_id = tag_registry->registerTag("no-network");
    addRule(ImplicationRule(
        "no-network-offline",
        LogicFormula::makeVar(no_network_id),
        LogicFormula::makeVar(offline_id),
        1.0f, "hardcoded"
    ));

    // Rule: local-only implies offline
    TagId local_only_id = tag_registry->registerTag("local-only");
    addRule(ImplicationRule(
        "local-only-offline",
        LogicFormula::makeVar(local_only_id),
        LogicFormula::makeVar(offline_id),
        1.0f, "hardcoded"
    ));

    // Mutually exclusive: cache-write-through and cache-write-back cannot coexist
    TagId write_through_id = tag_registry->registerTag("cache-write-through");
    TagId write_back_id = tag_registry->registerTag("cache-write-back");
    // This is expressed as: write-through implies not write-back
    addRule(ImplicationRule(
        "write-through-not-write-back",
        LogicFormula::makeVar(write_through_id),
        LogicFormula::makeNot(LogicFormula::makeVar(write_back_id)),
        1.0f, "hardcoded"
    ));
}

// Forward chaining inference
TagSet LogicEngine::inferTags(const TagSet& initial_tags, float min_confidence) const {
    TagSet result = initial_tags;
    bool changed = true;
    int max_iterations = 100;  // prevent infinite loops
    int iteration = 0;

    while(changed && iteration < max_iterations){
        changed = false;
        iteration++;

        for(const auto& rule : rules){
            if(rule.confidence < min_confidence) continue;

            // Check if premise is satisfied
            if(rule.premise->evaluate(result)){
                // Try to infer conclusion
                // For simple variable conclusions, add the tag
                if(rule.conclusion->op == LogicOp::VAR){
                    if(result.count(rule.conclusion->var_id) == 0){
                        result.insert(rule.conclusion->var_id);
                        changed = true;
                    }
                }
                // For NOT conclusions, check and flag conflicts
                else if(rule.conclusion->op == LogicOp::NOT &&
                        rule.conclusion->children[0]->op == LogicOp::VAR){
                    TagId forbidden_tag = rule.conclusion->children[0]->var_id;
                    if(result.count(forbidden_tag) > 0){
                        // Conflict detected - will be handled by checkConsistency
                        // For now, we don't add contradictory tags
                    }
                }
            }
        }
    }

    return result;
}

// Check for tag consistency
std::optional<LogicEngine::ConflictInfo> LogicEngine::checkConsistency(const TagSet& tags) const {
    for(const auto& rule : rules){
        if(rule.confidence < 0.95f) continue;  // Only check high-confidence rules

        // If premise is true but conclusion is false, we have a conflict
        if(rule.premise->evaluate(tags) && !rule.conclusion->evaluate(tags)){
            ConflictInfo conflict;
            conflict.description = String("Rule '") + rule.name + "' violated";

            // Collect conflicting tags
            if(rule.premise->op == LogicOp::VAR){
                conflict.conflicting_tags.Add(tag_registry->getTagName(rule.premise->var_id));
            }
            if(rule.conclusion->op == LogicOp::NOT &&
               rule.conclusion->children[0]->op == LogicOp::VAR){
                TagId forbidden = rule.conclusion->children[0]->var_id;
                if(tags.count(forbidden) > 0){
                    conflict.conflicting_tags.Add(tag_registry->getTagName(forbidden));
                }
            }

            // Generate suggestions
            conflict.suggestions.Add(String("Remove tag: ") + tag_registry->getTagName(rule.premise->var_id));
            if(rule.conclusion->op == LogicOp::VAR){
                conflict.suggestions.Add(String("Add tag: ") + tag_registry->getTagName(rule.conclusion->var_id));
            } else if(rule.conclusion->op == LogicOp::NOT &&
                      rule.conclusion->children[0]->op == LogicOp::VAR){
                conflict.suggestions.Add(String("Remove tag: ") +
                    tag_registry->getTagName(rule.conclusion->children[0]->var_id));
            }

            return conflict;
        }
    }
    return std::nullopt;
}

// Simple SAT solver for basic formulas (brute force for small tag sets)
bool LogicEngine::isSatisfiable(One<LogicFormula> formula) const {
    // Collect all variables in formula
    Index<TagId> vars;

    // Lambda to collect variables recursively
    auto collectVars = [&](const LogicFormula* f, auto& collectRef) -> void {
        if(f->op == LogicOp::VAR){
            vars.FindAdd(f->var_id);
        }
        for(const auto& child : f->children){
            collectRef(~child, collectRef);
        }
    };
    collectVars(~formula, collectVars);

    // Try all possible assignments (brute force for up to 20 variables)
    if(vars.GetCount() > 20) return true;  // Too large, assume satisfiable

    Vector<TagId> var_list;
    for(int i = 0; i < vars.GetCount(); i++){
        var_list.Add(vars[i]);
    }
    size_t n = var_list.GetCount();
    size_t total_assignments = 1ULL << n;

    for(size_t assignment = 0; assignment < total_assignments; assignment++){
        TagSet test_tags;
        for(size_t i = 0; i < n; i++){
            if(assignment & (1ULL << i)){
                test_tags.insert(var_list[(int)i]);
            }
        }
        if(formula->evaluate(test_tags)){
            return true;  // Found satisfying assignment
        }
    }

    return false;  // No satisfying assignment found
}

// Explain inference chain
Vector<String> LogicEngine::explainInference(TagId tag, const TagSet& initial_tags) const {
    Vector<String> explanation;

    // Check if tag is in initial set
    if(initial_tags.count(tag) > 0){
        explanation.Add(String("Tag '") + tag_registry->getTagName(tag) + "' was provided by user");
        return explanation;
    }

    // Find rules that could infer this tag
    for(const auto& rule : rules){
        if(rule.conclusion->op == LogicOp::VAR && rule.conclusion->var_id == tag){
            if(rule.premise->evaluate(initial_tags)){
                explanation.Add(String("Inferred via rule '") + rule.name + "': " +
                    rule.premise->toString(*tag_registry) + " => " +
                    rule.conclusion->toString(*tag_registry) +
                    " (confidence: " + AsString(int(rule.confidence * 100)) + "%, source: " + rule.source + ")");
            }
        }
    }

    if(explanation.IsEmpty()){
        explanation.Add(String("Tag '") + tag_registry->getTagName(tag) + "' cannot be inferred from given tags");
    }

    return explanation;
}

// Rule serialization format:
// name|premise_formula|conclusion_formula|confidence|source
// Formulas are serialized as S-expressions
String LogicEngine::serializeRule(const ImplicationRule& rule) const {
    return rule.name + "|" +
        rule.premise->toString(*tag_registry) + "|" +
        rule.conclusion->toString(*tag_registry) + "|" +
        AsString(rule.confidence) + "|" +
        rule.source;
}

// Helper function to parse formula from string (S-expression format)
static One<LogicFormula> parseFormulaFromString(const String& str, TagRegistry* reg) {
    // Simple recursive descent parser for logic formulas
    // Format: (not X), (and X Y...), (or X Y...), (implies X Y), tagname

    std::string str_std = str.ToStd();  // Convert to std::string for easier parsing

    auto trim = [](const std::string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };

    std::string trimmed = trim(str_std);

    if (trimmed.empty()) return One<LogicFormula>();

    // Check if it's a compound formula (starts with '(')
    if (trimmed[0] == '(') {
        // Find matching closing paren
        int depth = 0;
        size_t end = 0;
        for (size_t i = 0; i < trimmed.length(); ++i) {
            if (trimmed[i] == '(') depth++;
            else if (trimmed[i] == ')') {
                depth--;
                if (depth == 0) {
                    end = i;
                    break;
                }
            }
        }

        // Extract content inside parens
        std::string content = trim(trimmed.substr(1, end - 1));

        // Split by first space to get operator
        size_t space_pos = content.find(' ');
        if (space_pos == std::string::npos) return One<LogicFormula>();

        std::string op = content.substr(0, space_pos);
        std::string rest = trim(content.substr(space_pos + 1));

        if (op == "not") {
            auto child = parseFormulaFromString(String(rest.c_str()), reg);
            return child ? LogicFormula::makeNot(pick(child)) : One<LogicFormula>();
        } else if (op == "and") {
            // Parse multiple children
            Vector<One<LogicFormula>> children;
            size_t pos = 0;
            while (pos < rest.length()) {
                // Skip whitespace
                while (pos < rest.length() && std::isspace(rest[pos])) pos++;
                if (pos >= rest.length()) break;

                // Find next formula
                size_t start = pos;
                if (rest[pos] == '(') {
                    int d = 0;
                    while (pos < rest.length()) {
                        if (rest[pos] == '(') d++;
                        else if (rest[pos] == ')') {
                            d--;
                            if (d == 0) {
                                pos++;
                                break;
                            }
                        }
                        pos++;
                    }
                } else {
                    while (pos < rest.length() && !std::isspace(rest[pos]) && rest[pos] != ')') pos++;
                }

                std::string child_str = trim(rest.substr(start, pos - start));
                auto child = parseFormulaFromString(String(child_str.c_str()), reg);
                if (child) children.Add(pick(child));
            }
            return children.IsEmpty() ? One<LogicFormula>() : LogicFormula::makeAnd(pick(children));
        } else if (op == "or") {
            // Similar to 'and', parse multiple children
            Vector<One<LogicFormula>> children;
            size_t pos = 0;
            while (pos < rest.length()) {
                while (pos < rest.length() && std::isspace(rest[pos])) pos++;
                if (pos >= rest.length()) break;

                size_t start = pos;
                if (rest[pos] == '(') {
                    int d = 0;
                    while (pos < rest.length()) {
                        if (rest[pos] == '(') d++;
                        else if (rest[pos] == ')') {
                            d--;
                            if (d == 0) {
                                pos++;
                                break;
                            }
                        }
                        pos++;
                    }
                } else {
                    while (pos < rest.length() && !std::isspace(rest[pos]) && rest[pos] != ')') pos++;
                }

                std::string child_str = trim(rest.substr(start, pos - start));
                auto child = parseFormulaFromString(String(child_str.c_str()), reg);
                if (child) children.Add(pick(child));
            }
            return children.IsEmpty() ? One<LogicFormula>() : LogicFormula::makeOr(pick(children));
        } else if (op == "implies") {
            // Parse exactly 2 children
            size_t pos = 0;

            // First child
            while (pos < rest.length() && std::isspace(rest[pos])) pos++;
            size_t start1 = pos;
            if (rest[pos] == '(') {
                int d = 0;
                while (pos < rest.length()) {
                    if (rest[pos] == '(') d++;
                    else if (rest[pos] == ')') {
                        d--;
                        if (d == 0) {
                            pos++;
                            break;
                        }
                    }
                    pos++;
                }
            } else {
                while (pos < rest.length() && !std::isspace(rest[pos])) pos++;
            }
            std::string child1_str = trim(rest.substr(start1, pos - start1));

            // Second child
            while (pos < rest.length() && std::isspace(rest[pos])) pos++;
            std::string child2_str = trim(rest.substr(pos));

            auto lhs = parseFormulaFromString(String(child1_str.c_str()), reg);
            auto rhs = parseFormulaFromString(String(child2_str.c_str()), reg);
            return (lhs && rhs) ? LogicFormula::makeImplies(pick(lhs), pick(rhs)) : One<LogicFormula>();
        }

        return One<LogicFormula>();
    } else {
        // It's a tag name variable
        TagId id = reg->registerTag(String(trimmed.c_str()));
        return LogicFormula::makeVar(id);
    }
}

ImplicationRule LogicEngine::deserializeRule(const String& serialized) const {
    // Split by '|'
    Vector<String> parts = Split(serialized, '|');

    if (parts.GetCount() != 5) {
        throw Exc("invalid rule format: expected 5 parts separated by |");
    }

    String name = parts[0];
    auto premise = parseFormulaFromString(parts[1], tag_registry);
    auto conclusion = parseFormulaFromString(parts[2], tag_registry);
    float confidence = atof(~parts[3]);
    String source = parts[4];

    if (!premise || !conclusion) {
        throw Exc(String("failed to parse formula in rule: ") + name);
    }

    return ImplicationRule(pick(name), pick(premise), pick(conclusion), confidence, pick(source));
}

void LogicEngine::saveRulesToVfs(Vfs& vfs, const String& base_path) const {
    // Create /plan/rules directory structure
    // Note: mkdir creates parent directories automatically via ensureDirForOverlay
    vfs.mkdir(base_path.ToStd(), 0);
    vfs.mkdir((base_path + "/hardcoded").ToStd(), 0);
    vfs.mkdir((base_path + "/learned").ToStd(), 0);
    vfs.mkdir((base_path + "/ai-generated").ToStd(), 0);
    vfs.mkdir((base_path + "/user").ToStd(), 0);

    // Group rules by source
    VectorMap<String, Vector<const ImplicationRule*>> rules_by_source;
    for (int i = 0; i < rules.GetCount(); i++) {
        const ImplicationRule& rule = rules[i];
        int idx = rules_by_source.Find(rule.source);
        if(idx < 0) {
            idx = rules_by_source.GetCount();
            rules_by_source.Add(rule.source);
            rules_by_source[idx].Clear();  // Ensure it's initialized
        }
        rules_by_source[idx].Add(&rule);
    }

    // Write rules to appropriate files
    for(int i = 0; i < rules_by_source.GetCount(); i++) {
        String source = rules_by_source.GetKey(i);
        const Vector<const ImplicationRule*>& source_rules = rules_by_source[i];

        String content = String("# Logic rules - source: ") + source + "\n";
        content += "# Format: name|premise|conclusion|confidence|source\n\n";

        for (const auto* rule_ptr : source_rules) {
            content += serializeRule(*rule_ptr) + "\n";
        }

        String file_path = base_path + "/" + source + "/rules.txt";
        vfs.write(file_path.ToStd(), content.ToStd(), 0);
    }

    // Also create a summary file
    String summary = "# Logic Rules Summary\n\n";
    summary += String("Total rules: ") + AsString(rules.GetCount()) + "\n\n";

    for(int i = 0; i < rules_by_source.GetCount(); i++) {
        String source = rules_by_source.GetKey(i);
        const Vector<const ImplicationRule*>& source_rules = rules_by_source[i];

        summary += String("## ") + source + " (" + AsString(source_rules.GetCount()) + " rules)\n";
        for (const auto* rule_ptr : source_rules) {
            summary += String("  - ") + rule_ptr->name + " (confidence: " +
                    AsString(int(rule_ptr->confidence * 100)) + "%)\n";
        }
        summary += "\n";
    }

    vfs.write((base_path + "/summary.txt").ToStd(), summary.ToStd(), 0);
}

void LogicEngine::loadRulesFromVfs(Vfs& vfs, const String& base_path) {
    // Clear existing rules
    rules.Clear();

    // Load rules from each source directory
    Vector<String> sources = {"hardcoded", "learned", "ai-generated", "user"};

    for (const auto& source : sources) {
        String file_path = base_path + "/" + source + "/rules.txt";
        try {
            std::string content = vfs.read(file_path.ToStd(), std::nullopt);

            // Parse line by line
            Vector<String> lines = Split(String(content.c_str()), '\n');
            for(const String& line : lines) {
                // Skip empty lines and comments
                if (line.IsEmpty() || line[0] == '#') continue;

                try {
                    addRule(deserializeRule(line));
                } catch (Exc e) {
                    // Skip invalid rules, continue loading
                    LOG("Warning: skipping invalid rule in " << file_path << ": " << e);
                }
            }
        } catch (Exc) {
            // File doesn't exist or can't be read - that's OK
        }
    }
}

// Dynamic rule creation methods
void LogicEngine::addSimpleRule(const String& name, const String& premise_tag,
                                 const String& conclusion_tag, float confidence, const String& source) {
    TagId premise_id = tag_registry->registerTag(premise_tag);
    TagId conclusion_id = tag_registry->registerTag(conclusion_tag);

    addRule(ImplicationRule(
        clone(name),
        LogicFormula::makeVar(premise_id),
        LogicFormula::makeVar(conclusion_id),
        confidence,
        clone(source)
    ));
}

void LogicEngine::addExclusionRule(const String& name, const String& tag1,
                                    const String& tag2, const String& source) {
    // Exclusion: tag1 implies NOT tag2
    TagId tag1_id = tag_registry->registerTag(tag1);
    TagId tag2_id = tag_registry->registerTag(tag2);

    addRule(ImplicationRule(
        clone(name),
        LogicFormula::makeVar(tag1_id),
        LogicFormula::makeNot(LogicFormula::makeVar(tag2_id)),
        1.0f,
        clone(source)
    ));
}

void LogicEngine::removeRule(const String& name) {
    // Remove all rules with matching name
    for(int i = rules.GetCount() - 1; i >= 0; i--) {
        if(rules[i].name == name) {
            rules.Remove(i);
        }
    }
}

bool LogicEngine::hasRule(const String& name) const {
    for(int i = 0; i < rules.GetCount(); i++) {
        if(rules[i].name == name) return true;
    }
    return false;
}

