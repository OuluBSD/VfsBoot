#pragma once

#include "build_graph.h"

#include <map>
#include <set>
#include <string>
#include <vector>

// Minimal GNU Make-like parser that populates a generic BuildGraph
class MakeFile {
public:
    void parse(const std::string& content);
    BuildResult build(const std::string& target, Vfs& vfs, bool verbose = false);

    const std::map<std::string, BuildRule>& rules() const { return graph_.rules; }
    bool hasRule(const std::string& name) const { return graph_.rules.find(name) != graph_.rules.end(); }
    std::string firstRule() const { return rule_order_.empty() ? std::string() : rule_order_.front(); }

private:
    void parseLine(const std::string& line, std::string& current_target);
    std::string expandVariables(const std::string& text) const;
    std::string expandAutomaticVars(const std::string& text, const BuildRule& rule) const;
    void finalizePhonyMarkers();

    std::map<std::string, std::string> variables_;
    std::set<std::string> phony_targets_;
    std::vector<std::string> rule_order_;
    BuildGraph graph_;
};

