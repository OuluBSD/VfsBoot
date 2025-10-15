#include "VfsShell.h"

// ActionPlannerTest implementation
bool ActionPlannerTest::run(){
    try {
        passed = test_fn();
        if(!passed && error_message.empty()){
            error_message = "Test returned false";
        }
        return passed;
    } catch(const std::exception& e){
        passed = false;
        error_message = e.what();
        return false;
    }
}

// ActionPlannerTestSuite implementation
void ActionPlannerTestSuite::addTest(const std::string& name, const std::string& desc, std::function<bool()> fn){
    tests.emplace_back(name, desc, std::move(fn));
}

void ActionPlannerTestSuite::runAll(){
    for(auto& test : tests){
        test.run();
    }
}

void ActionPlannerTestSuite::printResults() const {
    std::cout << "\n=== Action Planner Test Results ===\n";
    size_t passed = 0, failed = 0;

    for(const auto& test : tests){
        if(test.passed){
            std::cout << "✓ " << test.name << "\n";
            ++passed;
        } else {
            std::cout << "✗ " << test.name << "\n";
            std::cout << "  " << test.description << "\n";
            if(!test.error_message.empty()){
                std::cout << "  Error: " << test.error_message << "\n";
            }
            ++failed;
        }
    }

    std::cout << "\nTotal: " << tests.size() << " tests, ";
    std::cout << passed << " passed, " << failed << " failed\n";
}

size_t ActionPlannerTestSuite::passedCount() const {
    return std::count_if(tests.begin(), tests.end(),
        [](const ActionPlannerTest& t){ return t.passed; });
}

size_t ActionPlannerTestSuite::failedCount() const {
    return std::count_if(tests.begin(), tests.end(),
        [](const ActionPlannerTest& t){ return !t.passed; });
}

//
// Hypothesis Testing System Implementation
//

// Hypothesis methods
void Hypothesis::addAssumption(const std::string& assumption){
    assumptions.push_back(assumption);
}

void Hypothesis::addValidation(const std::string& criterion){
    validation_criteria.push_back(criterion);
}

std::string Hypothesis::levelName() const {
    switch(level){
        case Level::SimpleQuery: return "Level 1: Simple Query";
        case Level::CodeModification: return "Level 2: Code Modification";
        case Level::Refactoring: return "Level 3: Refactoring";
        case Level::FeatureAddition: return "Level 4: Feature Addition";
        case Level::Architecture: return "Level 5: Architecture";
        default: return "Unknown Level";
    }
}

// HypothesisResult methods
void HypothesisResult::addFinding(const std::string& finding){
    findings.push_back(finding);
}

void HypothesisResult::addAction(const std::string& action){
    actions.push_back(action);
}

std::string HypothesisResult::summary() const {
    std::ostringstream oss;
    oss << "Success: " << (success ? "YES" : "NO") << "\n";
    oss << "Message: " << message << "\n";
    oss << "Nodes examined: " << nodes_examined << "\n";
    oss << "Nodes matched: " << nodes_matched << "\n";
    if(!findings.empty()){
        oss << "Findings:\n";
        for(const auto& f : findings){
            oss << "  - " << f << "\n";
        }
    }
    if(!actions.empty()){
        oss << "Proposed actions:\n";
        for(const auto& a : actions){
            oss << "  - " << a << "\n";
        }
    }
    return oss.str();
}

// HypothesisTester implementation

// Level 1: Simple query - find function/pattern in VFS
HypothesisResult HypothesisTester::testSimpleQuery(const std::string& target, const std::string& search_path){
    TRACE_FN("target=", target, " path=", search_path);
    HypothesisResult result;
    result.message = "Searching for '" + target + "' in VFS";

    // Use ContextBuilder to search
    context_builder.clear();
    context_builder.addFilter(ContextFilter::contentMatch(target));
    context_builder.collectFromPath(search_path);

    result.nodes_examined = context_builder.entryCount();
    result.nodes_matched = context_builder.entryCount();

    if(result.nodes_matched > 0){
        result.success = true;
        result.message = "Found " + std::to_string(result.nodes_matched) + " nodes containing '" + target + "'";

        for(size_t i = 0; i < std::min(size_t(10), context_builder.entries.size()); ++i){
            const auto& entry = context_builder.entries[i];
            result.addFinding("Found in: " + entry.vfs_path);
            result.addAction("Could examine: " + entry.vfs_path);
        }
    } else {
        result.message = "No nodes found containing '" + target + "'";
    }

    return result;
}

// Helper: Find function definitions in content
std::vector<std::string> HypothesisTester::findFunctionDefinitions(const std::string& path){
    TRACE_FN("path=", path);
    std::vector<std::string> functions;

    auto node = vfs.resolve(path);
    if(!node) return functions;

    std::string content = node->read();

    // Simple pattern matching for function definitions
    // Looks for: "type name(...)" patterns
    std::regex func_regex(R"(\b(\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
    std::smatch match;
    std::string::const_iterator search_start(content.cbegin());

    while(std::regex_search(search_start, content.cend(), match, func_regex)){
        std::string func_name = match[2].str();
        functions.push_back(func_name);
        search_start = match.suffix().first;
    }

    return functions;
}

// Helper: Find return paths in function content
std::vector<std::string> HypothesisTester::findReturnPaths(const std::string& function_content){
    TRACE_FN();
    std::vector<std::string> paths;

    // Find all return statements
    std::regex return_regex(R"(\breturn\s+([^;]+);)");
    std::smatch match;
    std::string::const_iterator search_start(function_content.cbegin());

    while(std::regex_search(search_start, function_content.cend(), match, return_regex)){
        paths.push_back(match[1].str());
        search_start = match.suffix().first;
    }

    return paths;
}

// Level 2: Code modification - validate error handling addition strategy
HypothesisResult HypothesisTester::testErrorHandlingAddition(const std::string& function_name,
                                                              const std::string& error_handling_style){
    TRACE_FN("function=", function_name, " style=", error_handling_style);
    HypothesisResult result;
    result.message = "Testing error handling addition for function '" + function_name + "'";

    // Search for the function
    context_builder.clear();
    context_builder.addFilter(ContextFilter::contentMatch(function_name));
    context_builder.collect();

    result.nodes_examined = context_builder.entryCount();

    if(context_builder.entryCount() == 0){
        result.message = "Function '" + function_name + "' not found";
        return result;
    }

    // Analyze each match
    for(const auto& entry : context_builder.entries){
        auto functions = findFunctionDefinitions(entry.vfs_path);

        for(const auto& func : functions){
            if(func.find(function_name) != std::string::npos){
                result.nodes_matched++;
                result.addFinding("Found function '" + func + "' in " + entry.vfs_path);

                // Analyze return paths
                auto returns = findReturnPaths(entry.content);
                result.addFinding("Found " + std::to_string(returns.size()) + " return paths");

                // Propose error handling strategy
                if(error_handling_style == "try-catch"){
                    result.addAction("Wrap function body in try-catch block in " + entry.vfs_path);
                    result.addAction("Add appropriate catch handlers for expected exceptions");
                } else if(error_handling_style == "error-code"){
                    result.addAction("Add error code checks before return statements");
                    result.addAction("Propagate error codes to caller");
                } else if(error_handling_style == "optional"){
                    result.addAction("Change return type to std::optional<T>");
                    result.addAction("Return std::nullopt on error paths");
                }

                result.success = true;
            }
        }
    }

    if(!result.success){
        result.message = "Could not identify error handling insertion points";
    } else {
        result.message = "Identified " + std::to_string(result.nodes_matched) + " insertion points for " + error_handling_style;
    }

    return result;
}

// Helper: Check if two content blocks are similar
bool HypothesisTester::contentSimilar(const std::string& a, const std::string& b, size_t min_lines){
    TRACE_FN("min_lines=", min_lines);

    // Split into lines
    auto split = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::istringstream iss(s);
        std::string line;
        while(std::getline(iss, line)){
            // Trim whitespace for comparison
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            if(!line.empty()){
                lines.push_back(line);
            }
        }
        return lines;
    };

    auto lines_a = split(a);
    auto lines_b = split(b);

    if(lines_a.size() < min_lines || lines_b.size() < min_lines){
        return false;
    }

    // Count matching lines
    size_t matches = 0;
    size_t max_check = std::min(lines_a.size(), lines_b.size());

    for(size_t i = 0; i < max_check; ++i){
        if(lines_a[i] == lines_b[i]){
            ++matches;
        }
    }

    // Require 80% similarity
    return matches >= (max_check * 4 / 5);
}

// Helper: Find duplicate code blocks
std::vector<std::pair<std::string, std::string>> HypothesisTester::findDuplicateBlocks(
    const std::string& path, size_t min_lines){
    TRACE_FN("path=", path, " min_lines=", min_lines);

    std::vector<std::pair<std::string, std::string>> duplicates;

    // Collect all file nodes
    context_builder.clear();
    context_builder.addFilter(ContextFilter::pathPrefix(path));
    context_builder.addFilter(ContextFilter::nodeKind(VfsNode::Kind::File));
    context_builder.collect();

    // Compare each pair of files
    const auto& entries = context_builder.entries;
    for(size_t i = 0; i < entries.size(); ++i){
        for(size_t j = i + 1; j < entries.size(); ++j){
            if(contentSimilar(entries[i].content, entries[j].content, min_lines)){
                duplicates.emplace_back(entries[i].vfs_path, entries[j].vfs_path);
            }
        }
    }

    return duplicates;
}

// Level 3: Refactoring - identify duplicate code for extraction
HypothesisResult HypothesisTester::testDuplicateExtraction(const std::string& search_path,
                                                            size_t min_similarity_lines){
    TRACE_FN("path=", search_path, " min_lines=", min_similarity_lines);
    HypothesisResult result;
    result.message = "Searching for duplicate code blocks (min " + std::to_string(min_similarity_lines) + " lines)";

    auto duplicates = findDuplicateBlocks(search_path, min_similarity_lines);

    result.nodes_examined = context_builder.entryCount();
    result.nodes_matched = duplicates.size();

    if(!duplicates.empty()){
        result.success = true;
        result.message = "Found " + std::to_string(duplicates.size()) + " duplicate code block pairs";

        for(const auto& [path1, path2] : duplicates){
            result.addFinding("Duplicate between: " + path1 + " and " + path2);
            result.addAction("Extract common code to shared helper function");
            result.addAction("Determine optimal parameter signature from usage");
            result.addAction("Update both locations to call extracted function");
        }
    } else {
        result.message = "No significant duplicate code blocks found";
    }

    return result;
}

// Helper: Find error paths (returns, throws, error codes)
std::vector<std::string> HypothesisTester::findErrorPaths(const std::string& path){
    TRACE_FN("path=", path);
    std::vector<std::string> error_paths;

    context_builder.clear();
    context_builder.addFilter(ContextFilter::pathPrefix(path));

    // Look for common error patterns
    std::vector<std::string> error_patterns = {
        "return.*nullptr",
        "return.*-1",
        "return.*false",
        "throw\\s+",
        "error",
        "fail"
    };

    for(const auto& pattern : error_patterns){
        context_builder.clear();
        context_builder.addFilter(ContextFilter::pathPrefix(path));
        context_builder.addFilter(ContextFilter::contentRegex(pattern));
        context_builder.collect();

        for(const auto& entry : context_builder.entries){
            error_paths.push_back(entry.vfs_path + " (pattern: " + pattern + ")");
        }
    }

    return error_paths;
}

// Level 4: Feature addition - plan logging instrumentation
HypothesisResult HypothesisTester::testLoggingInstrumentation(const std::string& search_path){
    TRACE_FN("path=", search_path);
    HypothesisResult result;
    result.message = "Analyzing error paths for logging instrumentation";

    auto error_paths = findErrorPaths(search_path);

    result.nodes_examined = context_builder.entryCount();
    result.nodes_matched = error_paths.size();

    if(!error_paths.empty()){
        result.success = true;
        result.message = "Identified " + std::to_string(error_paths.size()) + " potential logging points";

        for(const auto& path : error_paths){
            result.addFinding("Error path: " + path);
        }

        result.addAction("Add logging infrastructure (logger class or macros)");
        result.addAction("Insert log statements before error returns");
        result.addAction("Include function name, error type, and context in log messages");
        result.addAction("Tag all instrumented functions for tracking");
    } else {
        result.message = "No error paths found requiring instrumentation";
    }

    return result;
}

// Level 5: Architecture - evaluate design pattern applicability
HypothesisResult HypothesisTester::testArchitecturePattern(const std::string& pattern_name,
                                                            const std::string& target_path){
    TRACE_FN("pattern=", pattern_name, " path=", target_path);
    HypothesisResult result;
    result.message = "Evaluating " + pattern_name + " pattern for " + target_path;

    // Analyze structure
    context_builder.clear();
    context_builder.addFilter(ContextFilter::pathPrefix(target_path));
    context_builder.collect();

    result.nodes_examined = context_builder.entryCount();

    if(pattern_name == "visitor"){
        // Check for AST-like structure with multiple node types
        bool has_ast_nodes = false;
        bool has_inheritance = false;

        for(const auto& entry : context_builder.entries){
            if(entry.content.find("struct") != std::string::npos &&
               entry.content.find("Node") != std::string::npos){
                has_ast_nodes = true;
            }
            if(entry.content.find(": public") != std::string::npos ||
               entry.content.find(": VfsNode") != std::string::npos){
                has_inheritance = true;
            }
        }

        if(has_ast_nodes && has_inheritance){
            result.success = true;
            result.nodes_matched = context_builder.entryCount();
            result.addFinding("Found AST-like structure with inheritance hierarchy");
            result.addAction("Define Visitor base class with visit() methods for each node type");
            result.addAction("Add accept(Visitor&) method to base node class");
            result.addAction("Implement concrete visitors for specific traversal operations");
            result.addAction("Consider double-dispatch vs std::variant for type safety");
            result.addAction("Benchmark performance impact of visitor pattern");
            result.message = "Visitor pattern applicable - found suitable node hierarchy";
        } else {
            result.message = "Visitor pattern may not be applicable - missing node hierarchy";
        }
    } else if(pattern_name == "factory"){
        result.addFinding("Factory pattern analysis not yet implemented");
        result.addAction("Identify object creation patterns in codebase");
    } else if(pattern_name == "singleton"){
        result.addFinding("Singleton pattern analysis not yet implemented");
        result.addAction("Identify global state management patterns");
    } else {
        result.message = "Unknown pattern: " + pattern_name;
    }

    return result;
}

// General hypothesis testing interface
HypothesisResult HypothesisTester::test(Hypothesis& hypothesis){
    TRACE_FN("level=", static_cast<int>(hypothesis.level));

    hypothesis.tested = true;
    HypothesisResult result;

    // Route to appropriate test based on level
    switch(hypothesis.level){
        case Hypothesis::Level::SimpleQuery:
            // Extract target from goal
            result = testSimpleQuery(hypothesis.goal);
            break;
        case Hypothesis::Level::CodeModification:
            result = testErrorHandlingAddition(hypothesis.goal, "try-catch");
            break;
        case Hypothesis::Level::Refactoring:
            result = testDuplicateExtraction("/");
            break;
        case Hypothesis::Level::FeatureAddition:
            result = testLoggingInstrumentation("/");
            break;
        case Hypothesis::Level::Architecture:
            result = testArchitecturePattern("visitor", "/");
            break;
        default:
            result.message = "Unknown hypothesis level";
            result.success = false;
    }

    hypothesis.valid = result.success;
    hypothesis.result = result.summary();

    return result;
}

// HypothesisTestSuite implementation
void HypothesisTestSuite::addHypothesis(Hypothesis h){
    hypotheses.push_back(std::move(h));
}

void HypothesisTestSuite::runAll(){
    TRACE_FN();
    for(auto& hyp : hypotheses){
        auto result = tester.test(hyp);
        std::cout << "\n=== " << hyp.levelName() << " ===\n";
        std::cout << "Description: " << hyp.description << "\n";
        std::cout << "Goal: " << hyp.goal << "\n";
        std::cout << result.summary();
    }
}

void HypothesisTestSuite::printResults() const {
    std::cout << "\n=== Hypothesis Test Suite Results ===\n";
    size_t valid = validCount();
    size_t invalid = invalidCount();
    size_t untested = untestedCount();

    for(const auto& hyp : hypotheses){
        std::string status = hyp.tested ? (hyp.valid ? "✓ VALID" : "✗ INVALID") : "? UNTESTED";
        std::cout << status << " - " << hyp.levelName() << ": " << hyp.description << "\n";
    }

    std::cout << "\nTotal: " << hypotheses.size() << " hypotheses, ";
    std::cout << valid << " valid, " << invalid << " invalid, " << untested << " untested\n";
}

size_t HypothesisTestSuite::validCount() const {
    return std::count_if(hypotheses.begin(), hypotheses.end(),
        [](const Hypothesis& h){ return h.tested && h.valid; });
}

size_t HypothesisTestSuite::invalidCount() const {
    return std::count_if(hypotheses.begin(), hypotheses.end(),
        [](const Hypothesis& h){ return h.tested && !h.valid; });
}

size_t HypothesisTestSuite::untestedCount() const {
    return std::count_if(hypotheses.begin(), hypotheses.end(),
        [](const Hypothesis& h){ return !h.tested; });
}

void HypothesisTestSuite::createStandardSuite(){
    TRACE_FN();

    // Level 1: Simple query
    {
        Hypothesis h(Hypothesis::Level::SimpleQuery,
            "Find function 'foo' in VFS",
            "foo");
        h.addAssumption("Function exists somewhere in VFS");
        h.addValidation("At least one node contains 'foo'");
        addHypothesis(std::move(h));
    }

    // Level 2: Code modification
    {
        Hypothesis h(Hypothesis::Level::CodeModification,
            "Add error handling to function 'processData'",
            "processData");
        h.addAssumption("Function exists and has error-prone operations");
        h.addValidation("Return paths identified");
        h.addValidation("Error handling strategy applicable");
        addHypothesis(std::move(h));
    }

    // Level 3: Refactoring
    {
        Hypothesis h(Hypothesis::Level::Refactoring,
            "Extract duplicated code into helper functions",
            "/cpp");
        h.addAssumption("Multiple files contain similar code blocks");
        h.addValidation("At least one duplicate block pair found");
        h.addValidation("Common parameters can be inferred");
        addHypothesis(std::move(h));
    }

    // Level 4: Feature addition
    {
        Hypothesis h(Hypothesis::Level::FeatureAddition,
            "Add logging to all error paths",
            "/");
        h.addAssumption("Error paths exist (returns, throws, error codes)");
        h.addValidation("Error paths identified via pattern matching");
        h.addValidation("Logging infrastructure design proposed");
        addHypothesis(std::move(h));
    }

    // Level 5: Architecture
    {
        Hypothesis h(Hypothesis::Level::Architecture,
            "Implement visitor pattern for AST traversal",
            "visitor");
        h.addAssumption("AST has polymorphic node hierarchy");
        h.addValidation("Node types identified");
        h.addValidation("Visitor pattern design applicable");
        h.addValidation("Performance implications considered");
        addHypothesis(std::move(h));
    }
}

