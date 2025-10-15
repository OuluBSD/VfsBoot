#include "VfsShell.h"

// ============================================================================
// Feedback Pipeline Implementation
// ============================================================================

// MetricsCollector implementation
void MetricsCollector::startRun(const std::string& scenario_name) {
    TRACE_FN("scenario=", scenario_name);

    if (current_metrics) {
        // Finish previous run if not finished
        finishRun();
    }

    history.emplace_back();
    current_metrics = &history.back();
    current_metrics->scenario_name = scenario_name;
    current_metrics->timestamp = static_cast<uint64_t>(std::time(nullptr));
}

void MetricsCollector::recordRuleTrigger(const std::string& rule_name) {
    TRACE_FN("rule=", rule_name);
    if (!current_metrics) return;

    current_metrics->rules_triggered.push_back(rule_name);
    current_metrics->rules_applied++;
}

void MetricsCollector::recordRuleFailed(const std::string& rule_name) {
    TRACE_FN("rule=", rule_name);
    if (!current_metrics) return;

    current_metrics->rules_failed.push_back(rule_name);
}

void MetricsCollector::recordSuccess(bool success, const std::string& error) {
    TRACE_FN("success=", success);
    if (!current_metrics) return;

    current_metrics->success = success;
    current_metrics->error_message = error;
}

void MetricsCollector::recordIterations(size_t count) {
    if (!current_metrics) return;
    current_metrics->iterations = count;
}

void MetricsCollector::recordPerformance(double exec_time_ms, size_t tokens, size_t nodes) {
    if (!current_metrics) return;

    current_metrics->execution_time_ms = exec_time_ms;
    current_metrics->context_tokens_used = tokens;
    current_metrics->vfs_nodes_examined = nodes;
}

void MetricsCollector::recordOutcome(bool plan_matched, bool actions_completed, bool verification_passed) {
    if (!current_metrics) return;

    current_metrics->plan_matched_expected = plan_matched;
    current_metrics->actions_completed = actions_completed;
    current_metrics->verification_passed = verification_passed;
}

void MetricsCollector::finishRun() {
    TRACE_FN();
    current_metrics = nullptr;
}

std::vector<std::string> MetricsCollector::getMostTriggeredRules(size_t top_n) const {
    TRACE_FN("top_n=", top_n);

    std::map<std::string, size_t> rule_counts;
    for (const auto& metrics : history) {
        for (const auto& rule : metrics.rules_triggered) {
            rule_counts[rule]++;
        }
    }

    std::vector<std::pair<std::string, size_t>> sorted_rules(rule_counts.begin(), rule_counts.end());
    std::sort(sorted_rules.begin(), sorted_rules.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> result;
    for (size_t i = 0; i < std::min(top_n, sorted_rules.size()); ++i) {
        result.push_back(sorted_rules[i].first);
    }

    return result;
}

std::vector<std::string> MetricsCollector::getMostFailedRules(size_t top_n) const {
    TRACE_FN("top_n=", top_n);

    std::map<std::string, size_t> rule_counts;
    for (const auto& metrics : history) {
        for (const auto& rule : metrics.rules_failed) {
            rule_counts[rule]++;
        }
    }

    std::vector<std::pair<std::string, size_t>> sorted_rules(rule_counts.begin(), rule_counts.end());
    std::sort(sorted_rules.begin(), sorted_rules.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> result;
    for (size_t i = 0; i < std::min(top_n, sorted_rules.size()); ++i) {
        result.push_back(sorted_rules[i].first);
    }

    return result;
}

double MetricsCollector::getAverageSuccessRate() const {
    if (history.empty()) return 0.0;

    size_t success_count = 0;
    for (const auto& metrics : history) {
        if (metrics.success) success_count++;
    }

    return static_cast<double>(success_count) / history.size();
}

double MetricsCollector::getAverageIterations() const {
    if (history.empty()) return 0.0;

    size_t total = 0;
    for (const auto& metrics : history) {
        total += metrics.iterations;
    }

    return static_cast<double>(total) / history.size();
}

void MetricsCollector::saveToVfs(Vfs& vfs, const std::string& path) const {
    TRACE_FN("path=", path);

    vfs.mkdir(path, 0);

    std::ostringstream oss;
    oss << "# Planner Metrics History\n";
    oss << "# Total runs: " << history.size() << "\n\n";

    for (const auto& metrics : history) {
        oss << "[RUN]\n";
        oss << "scenario: " << metrics.scenario_name << "\n";
        oss << "timestamp: " << metrics.timestamp << "\n";
        oss << "success: " << (metrics.success ? "true" : "false") << "\n";
        oss << "iterations: " << metrics.iterations << "\n";
        oss << "rules_applied: " << metrics.rules_applied << "\n";
        oss << "execution_time_ms: " << metrics.execution_time_ms << "\n";
        oss << "context_tokens: " << metrics.context_tokens_used << "\n";
        oss << "vfs_nodes_examined: " << metrics.vfs_nodes_examined << "\n";
        oss << "plan_matched: " << (metrics.plan_matched_expected ? "true" : "false") << "\n";
        oss << "actions_completed: " << (metrics.actions_completed ? "true" : "false") << "\n";
        oss << "verification_passed: " << (metrics.verification_passed ? "true" : "false") << "\n";

        if (!metrics.error_message.empty()) {
            oss << "error: " << metrics.error_message << "\n";
        }

        if (!metrics.rules_triggered.empty()) {
            oss << "rules_triggered:";
            for (const auto& rule : metrics.rules_triggered) {
                oss << " " << rule;
            }
            oss << "\n";
        }

        if (!metrics.rules_failed.empty()) {
            oss << "rules_failed:";
            for (const auto& rule : metrics.rules_failed) {
                oss << " " << rule;
            }
            oss << "\n";
        }

        oss << "\n";
    }

    vfs.write(path + "/history.txt", oss.str(), 0);
}

void MetricsCollector::loadFromVfs(Vfs& vfs, const std::string& path) {
    TRACE_FN("path=", path);

    // TODO: Implement parsing of metrics history from VFS
    // For now, this is a stub
    try {
        std::string content = vfs.read(path + "/history.txt");
        (void)content;  // Suppress unused warning
    } catch (...) {
        // Ignore errors for now
    }
}

// RulePatch implementation
RulePatch RulePatch::addRule(const std::string& name,
                              std::shared_ptr<LogicFormula> premise,
                              std::shared_ptr<LogicFormula> conclusion,
                              float confidence,
                              const std::string& source,
                              const std::string& rationale) {
    RulePatch patch;
    patch.operation = Operation::Add;
    patch.rule_name = name;
    patch.new_premise = premise;
    patch.new_conclusion = conclusion;
    patch.new_confidence = confidence;
    patch.source = source;
    patch.rationale = rationale;
    return patch;
}

RulePatch RulePatch::modifyRule(const std::string& name,
                                std::shared_ptr<LogicFormula> new_premise,
                                std::shared_ptr<LogicFormula> new_conclusion,
                                float new_confidence,
                                const std::string& rationale) {
    RulePatch patch;
    patch.operation = Operation::Modify;
    patch.rule_name = name;
    patch.new_premise = new_premise;
    patch.new_conclusion = new_conclusion;
    patch.new_confidence = new_confidence;
    patch.source = "learned";
    patch.rationale = rationale;
    return patch;
}

RulePatch RulePatch::removeRule(const std::string& name, const std::string& rationale) {
    RulePatch patch;
    patch.operation = Operation::Remove;
    patch.rule_name = name;
    patch.rationale = rationale;
    return patch;
}

RulePatch RulePatch::adjustConfidence(const std::string& name,
                                      float new_confidence,
                                      const std::string& rationale) {
    RulePatch patch;
    patch.operation = Operation::AdjustConfidence;
    patch.rule_name = name;
    patch.new_confidence = new_confidence;
    patch.rationale = rationale;
    return patch;
}

std::string RulePatch::serialize(const TagRegistry& reg) const {
    std::ostringstream oss;

    // Format: operation|rule_name|premise|conclusion|confidence|source|rationale|evidence_count
    oss << static_cast<int>(operation) << "|";
    oss << rule_name << "|";

    if (new_premise) {
        oss << new_premise->toString(reg);
    }
    oss << "|";

    if (new_conclusion) {
        oss << new_conclusion->toString(reg);
    }
    oss << "|";

    oss << new_confidence << "|";
    oss << source << "|";
    oss << rationale << "|";
    oss << evidence_count;

    return oss.str();
}

RulePatch RulePatch::deserialize(const std::string& data, TagRegistry& reg) {
    // TODO: Implement full deserialization with formula parsing
    // For now, return a stub patch
    (void)data;
    (void)reg;
    return RulePatch();
}

// RulePatchStaging implementation
void RulePatchStaging::stagePatch(const RulePatch& patch) {
    TRACE_FN("rule=", patch.rule_name);
    pending_patches.push_back(patch);
}

void RulePatchStaging::stagePatch(RulePatch&& patch) {
    TRACE_FN("rule=", patch.rule_name);
    pending_patches.push_back(std::move(patch));
}

bool RulePatchStaging::applyPatch(size_t index) {
    TRACE_FN("index=", index);

    if (index >= pending_patches.size()) {
        return false;
    }

    const RulePatch& patch = pending_patches[index];

    try {
        switch (patch.operation) {
            case RulePatch::Operation::Add: {
                ImplicationRule rule(patch.rule_name, patch.new_premise, patch.new_conclusion,
                                    patch.new_confidence, patch.source);
                logic_engine->addRule(rule);
                break;
            }
            case RulePatch::Operation::Modify: {
                logic_engine->removeRule(patch.rule_name);
                ImplicationRule rule(patch.rule_name, patch.new_premise, patch.new_conclusion,
                                    patch.new_confidence, patch.source);
                logic_engine->addRule(rule);
                break;
            }
            case RulePatch::Operation::Remove: {
                logic_engine->removeRule(patch.rule_name);
                break;
            }
            case RulePatch::Operation::AdjustConfidence: {
                // For confidence adjustment, we need to find the rule and modify it
                // This requires removing and re-adding with new confidence
                // TODO: Add a more efficient way to just update confidence
                break;
            }
        }

        applied_patches.push_back(patch);
        pending_patches.erase(pending_patches.begin() + index);
        return true;

    } catch (const std::exception& e) {
        TRACE_MSG("Failed to apply patch: ", e.what());
        return false;
    }
}

bool RulePatchStaging::rejectPatch(size_t index, const std::string& reason) {
    TRACE_FN("index=", index, " reason=", reason);

    if (index >= pending_patches.size()) {
        return false;
    }

    rejected_patches.push_back(pending_patches[index]);
    pending_patches.erase(pending_patches.begin() + index);
    return true;
}

bool RulePatchStaging::applyAll() {
    TRACE_FN("count=", pending_patches.size());

    bool all_success = true;
    while (!pending_patches.empty()) {
        if (!applyPatch(0)) {
            all_success = false;
            rejectPatch(0, "Failed to apply");
        }
    }

    return all_success;
}

void RulePatchStaging::rejectAll() {
    TRACE_FN("count=", pending_patches.size());

    while (!pending_patches.empty()) {
        rejectPatch(0, "Batch rejection");
    }
}

void RulePatchStaging::clearPending() {
    pending_patches.clear();
}

void RulePatchStaging::clearApplied() {
    applied_patches.clear();
}

void RulePatchStaging::clearRejected() {
    rejected_patches.clear();
}

void RulePatchStaging::clearAll() {
    clearPending();
    clearApplied();
    clearRejected();
}

void RulePatchStaging::savePendingToVfs(Vfs& vfs, const std::string& path) const {
    TRACE_FN("path=", path);

    vfs.mkdir(path, 0);

    std::ostringstream oss;
    oss << "# Pending Rule Patches\n";
    oss << "# Count: " << pending_patches.size() << "\n\n";

    for (size_t i = 0; i < pending_patches.size(); ++i) {
        const auto& patch = pending_patches[i];
        oss << "[PATCH " << i << "]\n";
        oss << "operation: ";
        switch (patch.operation) {
            case RulePatch::Operation::Add: oss << "add\n"; break;
            case RulePatch::Operation::Modify: oss << "modify\n"; break;
            case RulePatch::Operation::Remove: oss << "remove\n"; break;
            case RulePatch::Operation::AdjustConfidence: oss << "adjust_confidence\n"; break;
        }
        oss << "rule_name: " << patch.rule_name << "\n";
        oss << "confidence: " << patch.new_confidence << "\n";
        oss << "source: " << patch.source << "\n";
        oss << "rationale: " << patch.rationale << "\n";
        oss << "evidence_count: " << patch.evidence_count << "\n\n";
    }

    vfs.write(path + "/pending.txt", oss.str(), 0);
}

void RulePatchStaging::saveAppliedToVfs(Vfs& vfs, const std::string& path) const {
    TRACE_FN("path=", path);

    vfs.mkdir(path, 0);

    std::ostringstream oss;
    oss << "# Applied Rule Patches\n";
    oss << "# Count: " << applied_patches.size() << "\n\n";

    for (size_t i = 0; i < applied_patches.size(); ++i) {
        const auto& patch = applied_patches[i];
        oss << "[PATCH " << i << "]\n";
        oss << "rule_name: " << patch.rule_name << "\n";
        oss << "operation: ";
        switch (patch.operation) {
            case RulePatch::Operation::Add: oss << "add\n"; break;
            case RulePatch::Operation::Modify: oss << "modify\n"; break;
            case RulePatch::Operation::Remove: oss << "remove\n"; break;
            case RulePatch::Operation::AdjustConfidence: oss << "adjust_confidence\n"; break;
        }
        oss << "rationale: " << patch.rationale << "\n\n";
    }

    vfs.write(path + "/applied.txt", oss.str(), 0);
}

void RulePatchStaging::loadPendingFromVfs(Vfs& vfs, const std::string& path) {
    TRACE_FN("path=", path);

    // TODO: Implement parsing of pending patches from VFS
    // For now, this is a stub
    try {
        std::string content = vfs.read(path + "/pending.txt");
        (void)content;  // Suppress unused warning
    } catch (...) {
        // Ignore errors for now
    }
}

// FeedbackLoop implementation
std::vector<FeedbackLoop::RulePattern> FeedbackLoop::detectPatterns() const {
    TRACE_FN();

    std::map<std::string, RulePattern> patterns;

    for (const auto& metrics : metrics_collector.history) {
        for (const auto& rule : metrics.rules_triggered) {
            auto& pattern = patterns[rule];
            pattern.rule_name = rule;
            pattern.trigger_count++;

            if (metrics.success) {
                pattern.success_count++;
            } else {
                pattern.failure_count++;
            }
        }

        for (const auto& rule : metrics.rules_failed) {
            auto& pattern = patterns[rule];
            pattern.rule_name = rule;
            pattern.failure_count++;
        }
    }

    std::vector<RulePattern> result;
    for (auto& [name, pattern] : patterns) {
        if (pattern.trigger_count > 0) {
            pattern.success_rate = static_cast<double>(pattern.success_count) / pattern.trigger_count;
        }
        result.push_back(pattern);
    }

    return result;
}

bool FeedbackLoop::shouldIncreaseConfidence(const RulePattern& pattern) const {
    return pattern.success_rate > 0.9 && pattern.trigger_count >= 5;
}

bool FeedbackLoop::shouldDecreaseConfidence(const RulePattern& pattern) const {
    return pattern.success_rate < 0.5 && pattern.trigger_count >= 3;
}

bool FeedbackLoop::shouldRemoveRule(const RulePattern& pattern) const {
    return pattern.success_rate < 0.2 && pattern.trigger_count >= 5;
}

std::vector<RulePatch> FeedbackLoop::analyzeMetrics(size_t min_evidence_count) {
    TRACE_FN("min_evidence=", min_evidence_count);

    std::vector<RulePatch> patches;
    auto patterns = detectPatterns();

    for (const auto& pattern : patterns) {
        if (pattern.trigger_count < min_evidence_count) {
            continue;
        }

        if (shouldRemoveRule(pattern)) {
            auto patch = RulePatch::removeRule(
                pattern.rule_name,
                "Low success rate: " + std::to_string(pattern.success_rate * 100) + "%");
            patch.evidence_count = pattern.trigger_count;
            patches.push_back(patch);
        } else if (shouldDecreaseConfidence(pattern)) {
            float new_confidence = static_cast<float>(pattern.success_rate * 0.8);
            auto patch = RulePatch::adjustConfidence(
                pattern.rule_name,
                new_confidence,
                "Below-average success rate: " + std::to_string(pattern.success_rate * 100) + "%");
            patch.evidence_count = pattern.trigger_count;
            patches.push_back(patch);
        } else if (shouldIncreaseConfidence(pattern)) {
            float new_confidence = std::min(1.0f, static_cast<float>(pattern.success_rate * 1.1));
            auto patch = RulePatch::adjustConfidence(
                pattern.rule_name,
                new_confidence,
                "High success rate: " + std::to_string(pattern.success_rate * 100) + "%");
            patch.evidence_count = pattern.trigger_count;
            patches.push_back(patch);
        }
    }

    return patches;
}

std::vector<RulePatch> FeedbackLoop::generatePatchesFromFailures() {
    TRACE_FN();

    // TODO: Implement sophisticated failure analysis
    // For now, return empty vector
    return {};
}

std::vector<RulePatch> FeedbackLoop::generatePatchesFromSuccesses() {
    TRACE_FN();

    // TODO: Implement success pattern mining
    // For now, return empty vector
    return {};
}

std::vector<RulePatch> FeedbackLoop::generatePatchesWithAI(const std::string& context_prompt) {
    TRACE_FN();

    if (!use_ai_assistance) {
        return {};
    }

    // TODO: Implement AI-assisted rule patch generation
    // For now, return empty vector
    (void)context_prompt;
    return {};
}

FeedbackLoop::FeedbackCycleResult FeedbackLoop::runCycle(bool auto_apply, size_t min_evidence) {
    TRACE_FN("auto_apply=", auto_apply, " min_evidence=", min_evidence);

    FeedbackCycleResult result;
    result.patches_generated = 0;
    result.patches_staged = 0;
    result.patches_applied = 0;

    // Analyze metrics and generate patches
    auto patches = analyzeMetrics(min_evidence);
    result.patches_generated = patches.size();

    // Stage all patches
    for (auto& patch : patches) {
        patch_staging.stagePatch(std::move(patch));
        result.patches_staged++;
    }

    // Auto-apply if requested
    if (auto_apply && result.patches_staged > 0) {
        if (patch_staging.applyAll()) {
            result.patches_applied = result.patches_staged;
        }
    }

    // Generate summary
    std::ostringstream summary;
    summary << "Feedback Cycle Complete:\n";
    summary << "  Patches Generated: " << result.patches_generated << "\n";
    summary << "  Patches Staged: " << result.patches_staged << "\n";
    summary << "  Patches Applied: " << result.patches_applied << "\n";
    result.summary = summary.str();

    return result;
}

void FeedbackLoop::interactiveReview() {
    TRACE_FN();

    // TODO: Implement interactive review UI
    // For now, just print pending patches
    std::cout << "=== Pending Rule Patches ===\n";
    auto pending = patch_staging.listPending();
    for (size_t i = 0; i < pending.size(); ++i) {
        std::cout << "[" << i << "] " << pending[i].rule_name << "\n";
        std::cout << "    Operation: ";
        switch (pending[i].operation) {
            case RulePatch::Operation::Add: std::cout << "Add\n"; break;
            case RulePatch::Operation::Modify: std::cout << "Modify\n"; break;
            case RulePatch::Operation::Remove: std::cout << "Remove\n"; break;
            case RulePatch::Operation::AdjustConfidence: std::cout << "Adjust Confidence\n"; break;
        }
        std::cout << "    Rationale: " << pending[i].rationale << "\n";
        std::cout << "    Evidence: " << pending[i].evidence_count << " runs\n\n";
    }
}

