#include "VfsShell.h"

// ====== Logic System for Tag Theorem Proving ======

// LogicFormula factory methods
std::shared_ptr<LogicFormula> LogicFormula::makeVar(TagId id){
    auto f = std::make_shared<LogicFormula>();
    f->op = LogicOp::VAR;
    f->var_id = id;
    return f;
}

std::shared_ptr<LogicFormula> LogicFormula::makeNot(std::shared_ptr<LogicFormula> f){
    auto result = std::make_shared<LogicFormula>();
    result->op = LogicOp::NOT;
    result->children.push_back(f);
    return result;
}

std::shared_ptr<LogicFormula> LogicFormula::makeAnd(std::vector<std::shared_ptr<LogicFormula>> fs){
    auto result = std::make_shared<LogicFormula>();
    result->op = LogicOp::AND;
    result->children = std::move(fs);
    return result;
}

std::shared_ptr<LogicFormula> LogicFormula::makeOr(std::vector<std::shared_ptr<LogicFormula>> fs){
    auto result = std::make_shared<LogicFormula>();
    result->op = LogicOp::OR;
    result->children = std::move(fs);
    return result;
}

std::shared_ptr<LogicFormula> LogicFormula::makeImplies(std::shared_ptr<LogicFormula> lhs, std::shared_ptr<LogicFormula> rhs){
    auto result = std::make_shared<LogicFormula>();
    result->op = LogicOp::IMPLIES;
    result->children.push_back(lhs);
    result->children.push_back(rhs);
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
std::string LogicFormula::toString(const TagRegistry& reg) const {
    switch(op){
        case LogicOp::VAR:
            return reg.getTagName(var_id);
        case LogicOp::NOT:
            return "(not " + children[0]->toString(reg) + ")";
        case LogicOp::AND:{
            std::string result = "(and";
            for(const auto& child : children){
                result += " " + child->toString(reg);
            }
            return result + ")";
        }
        case LogicOp::OR:{
            std::string result = "(or";
            for(const auto& child : children){
                result += " " + child->toString(reg);
            }
            return result + ")";
        }
        case LogicOp::IMPLIES:
            return "(implies " + children[0]->toString(reg) + " " + children[1]->toString(reg) + ")";
    }
    return "?";
}

// LogicEngine implementation
void LogicEngine::addRule(const ImplicationRule& rule){
    rules.push_back(rule);
}

void LogicEngine::addHardcodedRules(){
    // Rule: offline implies not network
    TagId offline_id = tag_registry->registerTag("offline");
    TagId network_id = tag_registry->registerTag("network");
    auto rule1 = ImplicationRule(
        "offline-no-network",
        LogicFormula::makeVar(offline_id),
        LogicFormula::makeNot(LogicFormula::makeVar(network_id)),
        1.0f, "hardcoded"
    );
    addRule(rule1);

    // Rule: fast usually implies cached (high confidence learned pattern)
    TagId fast_id = tag_registry->registerTag("fast");
    TagId cached_id = tag_registry->registerTag("cached");
    auto rule2 = ImplicationRule(
        "fast-cached",
        LogicFormula::makeVar(fast_id),
        LogicFormula::makeVar(cached_id),
        0.87f, "learned"
    );
    addRule(rule2);

    // Rule: cached implies not remote
    TagId remote_id = tag_registry->registerTag("remote");
    auto rule3 = ImplicationRule(
        "cached-not-remote",
        LogicFormula::makeVar(cached_id),
        LogicFormula::makeNot(LogicFormula::makeVar(remote_id)),
        1.0f, "hardcoded"
    );
    addRule(rule3);

    // Rule: no-network implies offline
    TagId no_network_id = tag_registry->registerTag("no-network");
    auto rule4 = ImplicationRule(
        "no-network-offline",
        LogicFormula::makeVar(no_network_id),
        LogicFormula::makeVar(offline_id),
        1.0f, "hardcoded"
    );
    addRule(rule4);

    // Rule: local-only implies offline
    TagId local_only_id = tag_registry->registerTag("local-only");
    auto rule5 = ImplicationRule(
        "local-only-offline",
        LogicFormula::makeVar(local_only_id),
        LogicFormula::makeVar(offline_id),
        1.0f, "hardcoded"
    );
    addRule(rule5);

    // Mutually exclusive: cache-write-through and cache-write-back cannot coexist
    TagId write_through_id = tag_registry->registerTag("cache-write-through");
    TagId write_back_id = tag_registry->registerTag("cache-write-back");
    // This is expressed as: write-through implies not write-back
    auto rule6 = ImplicationRule(
        "write-through-not-write-back",
        LogicFormula::makeVar(write_through_id),
        LogicFormula::makeNot(LogicFormula::makeVar(write_back_id)),
        1.0f, "hardcoded"
    );
    addRule(rule6);
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
            conflict.description = "Rule '" + rule.name + "' violated";

            // Collect conflicting tags
            if(rule.premise->op == LogicOp::VAR){
                conflict.conflicting_tags.push_back(tag_registry->getTagName(rule.premise->var_id));
            }
            if(rule.conclusion->op == LogicOp::NOT &&
               rule.conclusion->children[0]->op == LogicOp::VAR){
                TagId forbidden = rule.conclusion->children[0]->var_id;
                if(tags.count(forbidden) > 0){
                    conflict.conflicting_tags.push_back(tag_registry->getTagName(forbidden));
                }
            }

            // Generate suggestions
            conflict.suggestions.push_back("Remove tag: " + tag_registry->getTagName(rule.premise->var_id));
            if(rule.conclusion->op == LogicOp::VAR){
                conflict.suggestions.push_back("Add tag: " + tag_registry->getTagName(rule.conclusion->var_id));
            } else if(rule.conclusion->op == LogicOp::NOT &&
                      rule.conclusion->children[0]->op == LogicOp::VAR){
                conflict.suggestions.push_back("Remove tag: " +
                    tag_registry->getTagName(rule.conclusion->children[0]->var_id));
            }

            return conflict;
        }
    }
    return std::nullopt;
}

// Simple SAT solver for basic formulas (brute force for small tag sets)
bool LogicEngine::isSatisfiable(std::shared_ptr<LogicFormula> formula) const {
    // Collect all variables in formula
    std::set<TagId> vars;
    std::function<void(const LogicFormula*)> collectVars = [&](const LogicFormula* f){
        if(f->op == LogicOp::VAR){
            vars.insert(f->var_id);
        }
        for(const auto& child : f->children){
            collectVars(child.get());
        }
    };
    collectVars(formula.get());

    // Try all possible assignments (brute force for up to 20 variables)
    if(vars.size() > 20) return true;  // Too large, assume satisfiable

    std::vector<TagId> var_list(vars.begin(), vars.end());
    size_t n = var_list.size();
    size_t total_assignments = 1ULL << n;

    for(size_t assignment = 0; assignment < total_assignments; assignment++){
        TagSet test_tags;
        for(size_t i = 0; i < n; i++){
            if(assignment & (1ULL << i)){
                test_tags.insert(var_list[i]);
            }
        }
        if(formula->evaluate(test_tags)){
            return true;  // Found satisfying assignment
        }
    }

    return false;  // No satisfying assignment found
}

// Explain inference chain
std::vector<std::string> LogicEngine::explainInference(TagId tag, const TagSet& initial_tags) const {
    std::vector<std::string> explanation;

    // Check if tag is in initial set
    if(initial_tags.count(tag) > 0){
        explanation.push_back("Tag '" + tag_registry->getTagName(tag) + "' was provided by user");
        return explanation;
    }

    // Find rules that could infer this tag
    for(const auto& rule : rules){
        if(rule.conclusion->op == LogicOp::VAR && rule.conclusion->var_id == tag){
            if(rule.premise->evaluate(initial_tags)){
                explanation.push_back("Inferred via rule '" + rule.name + "': " +
                    rule.premise->toString(*tag_registry) + " => " +
                    rule.conclusion->toString(*tag_registry) +
                    " (confidence: " + std::to_string(int(rule.confidence * 100)) + "%, source: " + rule.source + ")");
            }
        }
    }

    if(explanation.empty()){
        explanation.push_back("Tag '" + tag_registry->getTagName(tag) + "' cannot be inferred from given tags");
    }

    return explanation;
}

// Rule serialization format:
// name|premise_formula|conclusion_formula|confidence|source
// Formulas are serialized as S-expressions
std::string LogicEngine::serializeRule(const ImplicationRule& rule) const {
    std::ostringstream oss;
    oss << rule.name << "|"
        << rule.premise->toString(*tag_registry) << "|"
        << rule.conclusion->toString(*tag_registry) << "|"
        << rule.confidence << "|"
        << rule.source;
    return oss.str();
}

// Helper function to parse formula from string (S-expression format)
static std::shared_ptr<LogicFormula> parseFormulaFromString(const std::string& str, TagRegistry* reg) {
    // Simple recursive descent parser for logic formulas
    // Format: (not X), (and X Y...), (or X Y...), (implies X Y), tagname

    auto trim = [](const std::string& s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };

    std::string trimmed = trim(str);

    if (trimmed.empty()) return nullptr;

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
        if (space_pos == std::string::npos) return nullptr;

        std::string op = content.substr(0, space_pos);
        std::string rest = trim(content.substr(space_pos + 1));

        if (op == "not") {
            auto child = parseFormulaFromString(rest, reg);
            return child ? LogicFormula::makeNot(child) : nullptr;
        } else if (op == "and") {
            // Parse multiple children
            std::vector<std::shared_ptr<LogicFormula>> children;
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
                auto child = parseFormulaFromString(child_str, reg);
                if (child) children.push_back(child);
            }
            return children.empty() ? nullptr : LogicFormula::makeAnd(children);
        } else if (op == "or") {
            // Similar to 'and', parse multiple children
            std::vector<std::shared_ptr<LogicFormula>> children;
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
                auto child = parseFormulaFromString(child_str, reg);
                if (child) children.push_back(child);
            }
            return children.empty() ? nullptr : LogicFormula::makeOr(children);
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

            auto lhs = parseFormulaFromString(child1_str, reg);
            auto rhs = parseFormulaFromString(child2_str, reg);
            return (lhs && rhs) ? LogicFormula::makeImplies(lhs, rhs) : nullptr;
        }

        return nullptr;
    } else {
        // It's a tag name variable
        TagId id = reg->registerTag(trimmed);
        return LogicFormula::makeVar(id);
    }
}

ImplicationRule LogicEngine::deserializeRule(const std::string& serialized) const {
    // Split by '|'
    std::vector<std::string> parts;
    size_t start = 0;
    size_t pos = 0;
    while ((pos = serialized.find('|', start)) != std::string::npos) {
        parts.push_back(serialized.substr(start, pos - start));
        start = pos + 1;
    }
    parts.push_back(serialized.substr(start));

    if (parts.size() != 5) {
        throw std::runtime_error("invalid rule format: expected 5 parts separated by |");
    }

    std::string name = parts[0];
    auto premise = parseFormulaFromString(parts[1], tag_registry);
    auto conclusion = parseFormulaFromString(parts[2], tag_registry);
    float confidence = std::stof(parts[3]);
    std::string source = parts[4];

    if (!premise || !conclusion) {
        throw std::runtime_error("failed to parse formula in rule: " + name);
    }

    return ImplicationRule(name, premise, conclusion, confidence, source);
}

void LogicEngine::saveRulesToVfs(Vfs& vfs, const std::string& base_path) const {
    // Create /plan/rules directory structure
    // Note: mkdir creates parent directories automatically via ensureDirForOverlay
    vfs.mkdir(base_path, 0);
    vfs.mkdir(base_path + "/hardcoded", 0);
    vfs.mkdir(base_path + "/learned", 0);
    vfs.mkdir(base_path + "/ai-generated", 0);
    vfs.mkdir(base_path + "/user", 0);

    // Group rules by source
    std::map<std::string, std::vector<const ImplicationRule*>> rules_by_source;
    for (const auto& rule : rules) {
        rules_by_source[rule.source].push_back(&rule);
    }

    // Write rules to appropriate files
    for (const auto& [source, source_rules] : rules_by_source) {
        std::ostringstream content;
        content << "# Logic rules - source: " << source << "\n";
        content << "# Format: name|premise|conclusion|confidence|source\n\n";

        for (const auto* rule_ptr : source_rules) {
            content << serializeRule(*rule_ptr) << "\n";
        }

        std::string file_path = base_path + "/" + source + "/rules.txt";
        vfs.write(file_path, content.str(), 0);
    }

    // Also create a summary file
    std::ostringstream summary;
    summary << "# Logic Rules Summary\n\n";
    summary << "Total rules: " << rules.size() << "\n\n";

    for (const auto& [source, source_rules] : rules_by_source) {
        summary << "## " << source << " (" << source_rules.size() << " rules)\n";
        for (const auto* rule_ptr : source_rules) {
            summary << "  - " << rule_ptr->name << " (confidence: "
                    << static_cast<int>(rule_ptr->confidence * 100) << "%)\n";
        }
        summary << "\n";
    }

    vfs.write(base_path + "/summary.txt", summary.str(), 0);
}

void LogicEngine::loadRulesFromVfs(Vfs& vfs, const std::string& base_path) {
    // Clear existing rules
    rules.clear();

    // Load rules from each source directory
    std::vector<std::string> sources = {"hardcoded", "learned", "ai-generated", "user"};

    for (const auto& source : sources) {
        std::string file_path = base_path + "/" + source + "/rules.txt";
        try {
            std::string content = vfs.read(file_path, std::nullopt);

            // Parse line by line
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line)) {
                // Skip empty lines and comments
                if (line.empty() || line[0] == '#') continue;

                try {
                    ImplicationRule rule = deserializeRule(line);
                    addRule(rule);
                } catch (const std::exception& e) {
                    // Skip invalid rules, continue loading
                    std::cerr << "Warning: skipping invalid rule in " << file_path
                              << ": " << e.what() << "\n";
                }
            }
        } catch (const std::exception&) {
            // File doesn't exist or can't be read - that's OK
        }
    }
}

// Dynamic rule creation methods
void LogicEngine::addSimpleRule(const std::string& name, const std::string& premise_tag,
                                 const std::string& conclusion_tag, float confidence, const std::string& source) {
    TagId premise_id = tag_registry->registerTag(premise_tag);
    TagId conclusion_id = tag_registry->registerTag(conclusion_tag);

    auto premise = LogicFormula::makeVar(premise_id);
    auto conclusion = LogicFormula::makeVar(conclusion_id);

    ImplicationRule rule(name, premise, conclusion, confidence, source);
    addRule(rule);
}

void LogicEngine::addExclusionRule(const std::string& name, const std::string& tag1,
                                    const std::string& tag2, const std::string& source) {
    // Exclusion: tag1 implies NOT tag2
    TagId tag1_id = tag_registry->registerTag(tag1);
    TagId tag2_id = tag_registry->registerTag(tag2);

    auto premise = LogicFormula::makeVar(tag1_id);
    auto conclusion = LogicFormula::makeNot(LogicFormula::makeVar(tag2_id));

    ImplicationRule rule(name, premise, conclusion, 1.0f, source);
    addRule(rule);
}

void LogicEngine::removeRule(const std::string& name) {
    auto it = std::remove_if(rules.begin(), rules.end(),
                             [&name](const ImplicationRule& r) { return r.name == name; });
    rules.erase(it, rules.end());
}

bool LogicEngine::hasRule(const std::string& name) const {
    return std::any_of(rules.begin(), rules.end(),
                      [&name](const ImplicationRule& r) { return r.name == name; });
}

