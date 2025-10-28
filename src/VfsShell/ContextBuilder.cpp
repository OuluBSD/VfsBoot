#include "VfsShell.h"

// ====== ACTION PLANNER CONTEXT BUILDER ======
//

// ContextFilter factory methods
ContextFilter ContextFilter::tagAny(const TagSet& t){
    ContextFilter f;
    f.type = Type::TagAny;
    f.tags = t;
    return f;
}

ContextFilter ContextFilter::tagAll(const TagSet& t){
    ContextFilter f;
    f.type = Type::TagAll;
    f.tags = t;
    return f;
}

ContextFilter ContextFilter::tagNone(const TagSet& t){
    ContextFilter f;
    f.type = Type::TagNone;
    f.tags = t;
    return f;
}

ContextFilter ContextFilter::pathPrefix(const std::string& prefix){
    ContextFilter f;
    f.type = Type::PathPrefix;
    f.pattern = prefix;
    return f;
}

ContextFilter ContextFilter::pathPattern(const std::string& pattern){
    ContextFilter f;
    f.type = Type::PathPattern;
    f.pattern = pattern;
    return f;
}

ContextFilter ContextFilter::contentMatch(const std::string& substr){
    ContextFilter f;
    f.type = Type::ContentMatch;
    f.pattern = substr;
    return f;
}

ContextFilter ContextFilter::contentRegex(const std::string& regex){
    ContextFilter f;
    f.type = Type::ContentRegex;
    f.pattern = regex;
    return f;
}

ContextFilter ContextFilter::nodeKind(VfsNode::Kind k){
    ContextFilter f;
    f.type = Type::NodeKind;
    f.kind = k;
    return f;
}

ContextFilter ContextFilter::custom(std::function<bool(VfsNode*)> pred){
    ContextFilter f;
    f.type = Type::Custom;
    f.predicate = std::move(pred);
    return f;
}

bool ContextFilter::matches(VfsNode* node, const std::string& path, Vfs& vfs) const {
    if(!node) return false;

    switch(type){
        case Type::TagAny: {
            const TagSet* node_tags = vfs.tag_storage.getTags(node);
            if(!node_tags) return false;
            for(TagId tag : tags){
                if(node_tags->count(tag) > 0) return true;
            }
            return false;
        }
        case Type::TagAll: {
            const TagSet* node_tags = vfs.tag_storage.getTags(node);
            if(!node_tags) return false;
            for(TagId tag : tags){
                if(node_tags->count(tag) == 0) return false;
            }
            return true;
        }
        case Type::TagNone: {
            const TagSet* node_tags = vfs.tag_storage.getTags(node);
            if(!node_tags) return true;
            for(TagId tag : tags){
                if(node_tags->count(tag) > 0) return false;
            }
            return true;
        }
        case Type::PathPrefix:
            return path.find(pattern) == 0;
        case Type::PathPattern: {
            // Simple glob pattern matching (* and ?)
            std::string glob_pattern = pattern;
            std::string regex_str = glob_pattern;
            size_t pos = 0;
            while((pos = regex_str.find("*", pos)) != std::string::npos){
                regex_str.replace(pos, 1, ".*");
                pos += 2;
            }
            pos = 0;
            while((pos = regex_str.find("?", pos)) != std::string::npos){
                regex_str.replace(pos, 1, ".");
                pos += 1;
            }
            try {
                return std::regex_match(path, std::regex(regex_str));
            } catch(...) {
                return false;
            }
        }
        case Type::ContentMatch: {
            std::string content = node->read();
            return content.find(pattern) != std::string::npos;
        }
        case Type::ContentRegex: {
            std::string content = node->read();
            try {
                return std::regex_search(content, std::regex(pattern));
            } catch(...) {
                return false;
            }
        }
        case Type::NodeKind:
            return node->kind == kind;
        case Type::Custom:
            return predicate ? predicate(node) : false;
    }
    return false;
}

// ContextEntry token estimation
size_t ContextEntry::estimate_tokens(const std::string& text){
    // Rough estimate: ~4 chars per token (GPT-style tokenization)
    return (text.size() + 3) / 4;
}

// ContextBuilder implementation
void ContextBuilder::addFilter(const ContextFilter& filter){
    filters.push_back(filter);
}

void ContextBuilder::addFilter(ContextFilter&& filter){
    filters.push_back(std::move(filter));
}

void ContextBuilder::collect(){
    collectFromPath("/");
}

void ContextBuilder::collectFromPath(const std::string& root_path){
    // Use resolveMulti to handle multiple overlays
    auto hits = vfs.resolveMulti(root_path);
    for(const auto& hit : hits){
        if(hit.node){
            visitNode(root_path, hit.node.get());
        }
    }
}

void ContextBuilder::visitNode(const std::string& path, VfsNode* node){
    if(!node) return;

    // Check if node matches any filter
    if(matchesAnyFilter(path, node)){
        std::string content = node->read();
        int priority = 100;  // Default priority

        // Higher priority for tagged nodes with "important" or "critical"
        const TagSet* tags = tag_storage.getTags(node);
        if(tags){
            TagId important_id = tag_registry.getTagId("important");
            TagId critical_id = tag_registry.getTagId("critical");
            if(tags->count(critical_id) > 0) priority = 200;
            else if(tags->count(important_id) > 0) priority = 150;
        }

        ContextEntry entry(path, node, content, priority);
        if(tags) entry.tags = *tags;
        entries.push_back(std::move(entry));
    }

    // Recurse into children
    if(node->isDir()){
        for(const auto& [name, child] : node->children()){
            std::string child_path = path;
            if(child_path.back() != '/') child_path += '/';
            child_path += name;
            visitNode(child_path, child.get());
        }
    }
}

bool ContextBuilder::matchesAnyFilter(const std::string& path, VfsNode* node) const {
    if(filters.empty()) return true;  // No filters = match all
    for(const auto& filter : filters){
        if(filter.matches(node, path, vfs)) return true;
    }
    return false;
}

std::string ContextBuilder::build(){
    std::ostringstream oss;
    size_t current_tokens = 0;

    for(const auto& entry : entries){
        if(current_tokens + entry.token_estimate > max_tokens) break;

        oss << "=== " << entry.vfs_path << " ===\n";
        if(!entry.tags.empty()){
            oss << "Tags: ";
            bool first = true;
            for(TagId tag : entry.tags){
                if(!first) oss << ", ";
                oss << tag_registry.getTagName(tag);
                first = false;
            }
            oss << "\n";
        }
        oss << entry.content << "\n\n";
        current_tokens += entry.token_estimate;
    }

    return oss.str();
}

std::string ContextBuilder::buildWithPriority(){
    // Sort entries by priority (descending)
    std::sort(entries.begin(), entries.end(),
        [](const ContextEntry& a, const ContextEntry& b){
            return a.priority > b.priority;
        });
    return build();
}

size_t ContextBuilder::totalTokens() const {
    size_t total = 0;
    for(const auto& entry : entries){
        total += entry.token_estimate;
    }
    return total;
}

void ContextBuilder::clear(){
    entries.clear();
    filters.clear();
    seen_content.clear();
}

// Advanced context builder features
std::string ContextBuilder::buildWithOptions(const ContextOptions& opts){
    TRACE_FN("opts.deduplicate=", opts.deduplicate, ", opts.hierarchical=", opts.hierarchical);

    // Deduplication
    if(opts.deduplicate){
        deduplicateEntries();
    }

    // Hierarchical mode
    if(opts.hierarchical){
        auto [overview, details] = buildHierarchical();
        return overview + "\n\n" + details;
    }

    // Adaptive budget
    size_t effective_budget = max_tokens;
    if(opts.adaptive_budget){
        size_t total = totalTokens();
        if(total > max_tokens * 2){
            effective_budget = max_tokens * 2;  // Allow more space for complex contexts
        }
    }

    // Include dependencies
    if(opts.include_dependencies){
        std::vector<ContextEntry> deps;
        for(const auto& entry : entries){
            auto entry_deps = getDependencies(entry);
            deps.insert(deps.end(), entry_deps.begin(), entry_deps.end());
        }
        entries.insert(entries.end(), deps.begin(), deps.end());
    }

    // Build with summarization
    std::ostringstream oss;
    size_t current_tokens = 0;
    for(auto& entry : entries){
        // Summarize large entries if threshold is set
        if(opts.summary_threshold > 0 &&
           entry.token_estimate > static_cast<size_t>(opts.summary_threshold)){
            size_t target = opts.summary_threshold / 2;
            std::string summary = summarizeEntry(entry, target);
            current_tokens += target;
            oss << "=== " << entry.vfs_path << " (summarized) ===\n";
            oss << summary << "\n\n";
        } else {
            if(current_tokens + entry.token_estimate > effective_budget) break;
            oss << "=== " << entry.vfs_path << " ===\n";
            if(!entry.tags.empty()){
                oss << "Tags: ";
                bool first = true;
                for(TagId tag : entry.tags){
                    if(!first) oss << ", ";
                    oss << tag_registry.getTagName(tag);
                    first = false;
                }
                oss << "\n";
            }
            oss << entry.content << "\n\n";
            current_tokens += entry.token_estimate;
        }
    }

    return oss.str();
}

std::vector<ContextEntry> ContextBuilder::getDependencies(const ContextEntry& entry){
    TRACE_FN("entry.vfs_path=", entry.vfs_path);
    std::vector<ContextEntry> deps;

    // TODO: Add dependency tracking (imports, includes, link nodes, etc.)
    // For now, return empty dependencies
    (void)entry;  // Suppress unused parameter warning

    return deps;
}

std::string ContextBuilder::summarizeEntry(const ContextEntry& entry, size_t /* target_tokens */){
    TRACE_FN("entry.vfs_path=", entry.vfs_path);

    // Simple summarization: take first and last portions
    std::string content = entry.content;
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while(std::getline(iss, line)){
        lines.push_back(line);
    }

    if(lines.size() <= 20) return content;  // Too small to summarize

    // Take first 10 and last 10 lines
    std::ostringstream oss;
    for(size_t i = 0; i < 10 && i < lines.size(); ++i){
        oss << lines[i] << "\n";
    }
    oss << "\n... (" << (lines.size() - 20) << " lines omitted) ...\n\n";
    for(size_t i = lines.size() - 10; i < lines.size(); ++i){
        oss << lines[i] << "\n";
    }

    return oss.str();
}

void ContextBuilder::deduplicateEntries(){
    TRACE_FN("entries.size()=", entries.size());

    std::vector<ContextEntry> unique_entries;
    for(auto& entry : entries){
        std::string content_hash = compute_string_hash(entry.content);
        if(seen_content.find(content_hash) == seen_content.end()){
            seen_content.insert(content_hash);
            unique_entries.push_back(std::move(entry));
        }
    }

    entries = std::move(unique_entries);
}

std::pair<std::string, std::string> ContextBuilder::buildHierarchical(){
    TRACE_FN("entries.size()=", entries.size());

    // Overview: just paths and tags
    std::ostringstream overview_oss;
    overview_oss << "=== Context Overview ===\n";
    for(const auto& entry : entries){
        overview_oss << entry.vfs_path;
        if(!entry.tags.empty()){
            overview_oss << " [";
            bool first = true;
            for(TagId tag : entry.tags){
                if(!first) overview_oss << ",";
                overview_oss << tag_registry.getTagName(tag);
                first = false;
            }
            overview_oss << "]";
        }
        overview_oss << "\n";
    }

    // Details: full content
    std::ostringstream details_oss;
    details_oss << "=== Context Details ===\n";
    size_t current_tokens = 0;
    for(const auto& entry : entries){
        if(current_tokens + entry.token_estimate > max_tokens) break;
        details_oss << "\n--- " << entry.vfs_path << " ---\n";
        details_oss << entry.content << "\n";
        current_tokens += entry.token_estimate;
    }

    return {overview_oss.str(), details_oss.str()};
}

void ContextBuilder::addCompoundFilter(FilterLogic logic, const std::vector<ContextFilter>& subfilters){
    TRACE_FN("logic=", static_cast<int>(logic), ", subfilters.size()=", subfilters.size());

    // Create a compound filter using Custom type
    ContextFilter compound;
    compound.type = ContextFilter::Type::Custom;

    switch(logic){
        case FilterLogic::And:
            compound.predicate = [this, subfilters](VfsNode* node){
                std::string dummy_path;  // We need path context but don't have it in predicate
                for(const auto& f : subfilters){
                    if(!f.matches(node, dummy_path, vfs)) return false;
                }
                return true;
            };
            break;

        case FilterLogic::Or:
            compound.predicate = [this, subfilters](VfsNode* node){
                std::string dummy_path;
                for(const auto& f : subfilters){
                    if(f.matches(node, dummy_path, vfs)) return true;
                }
                return false;
            };
            break;

        case FilterLogic::Not:
            if(subfilters.size() != 1){
                throw std::runtime_error("NOT filter requires exactly one subfilter");
            }
            compound.predicate = [this, subfilters](VfsNode* node){
                std::string dummy_path;
                return !subfilters[0].matches(node, dummy_path, vfs);
            };
            break;
    }

    filters.push_back(compound);
}

// ReplacementStrategy factory methods
ReplacementStrategy ReplacementStrategy::replaceAll(const std::string& path, const std::string& content){
    ReplacementStrategy s;
    s.type = Type::ReplaceAll;
    s.target_path = path;
    s.replacement = content;
    return s;
}

ReplacementStrategy ReplacementStrategy::replaceRange(const std::string& path, size_t start, size_t end, const std::string& content){
    ReplacementStrategy s;
    s.type = Type::ReplaceRange;
    s.target_path = path;
    s.start_line = start;
    s.end_line = end;
    s.replacement = content;
    return s;
}

ReplacementStrategy ReplacementStrategy::replaceFunction(const std::string& path, const std::string& func_name, const std::string& content){
    ReplacementStrategy s;
    s.type = Type::ReplaceFunction;
    s.target_path = path;
    s.identifier = func_name;
    s.replacement = content;
    return s;
}

ReplacementStrategy ReplacementStrategy::insertBefore(const std::string& path, const std::string& pattern, const std::string& content){
    ReplacementStrategy s;
    s.type = Type::InsertBefore;
    s.target_path = path;
    s.match_pattern = pattern;
    s.replacement = content;
    return s;
}

ReplacementStrategy ReplacementStrategy::insertAfter(const std::string& path, const std::string& pattern, const std::string& content){
    ReplacementStrategy s;
    s.type = Type::InsertAfter;
    s.target_path = path;
    s.match_pattern = pattern;
    s.replacement = content;
    return s;
}

ReplacementStrategy ReplacementStrategy::deleteMatching(const std::string& path, const std::string& pattern){
    ReplacementStrategy s;
    s.type = Type::DeleteMatching;
    s.target_path = path;
    s.match_pattern = pattern;
    return s;
}

ReplacementStrategy ReplacementStrategy::commentOut(const std::string& path, const std::string& pattern){
    ReplacementStrategy s;
    s.type = Type::CommentOut;
    s.target_path = path;
    s.match_pattern = pattern;
    return s;
}

bool ReplacementStrategy::apply(Vfs& vfs) const {
    auto node = vfs.resolve(target_path);
    if(!node) return false;

    std::string content = node->read();
    std::istringstream iss(content);
    std::vector<std::string> lines;
    std::string line;
    while(std::getline(iss, line)){
        lines.push_back(line);
    }

    switch(type){
        case Type::ReplaceAll:
            node->write(replacement);
            return true;

        case Type::ReplaceRange: {
            if(start_line >= lines.size() || end_line >= lines.size()) return false;
            std::ostringstream oss;
            for(size_t i = 0; i < start_line; ++i){
                oss << lines[i] << "\n";
            }
            oss << replacement;
            for(size_t i = end_line + 1; i < lines.size(); ++i){
                oss << lines[i] << "\n";
            }
            node->write(oss.str());
            return true;
        }

        case Type::ReplaceFunction: {
            // Simple function replacement: find "type identifier(" pattern
            std::regex func_regex(R"(\w+\s+)" + identifier + R"(\s*\([^)]*\)\s*\{)");
            std::string new_content = std::regex_replace(content, func_regex, replacement);
            node->write(new_content);
            return true;
        }

        case Type::InsertBefore: {
            std::ostringstream oss;
            for(const auto& l : lines){
                if(l.find(match_pattern) != std::string::npos){
                    oss << replacement << "\n";
                }
                oss << l << "\n";
            }
            node->write(oss.str());
            return true;
        }

        case Type::InsertAfter: {
            std::ostringstream oss;
            for(const auto& l : lines){
                oss << l << "\n";
                if(l.find(match_pattern) != std::string::npos){
                    oss << replacement << "\n";
                }
            }
            node->write(oss.str());
            return true;
        }

        case Type::DeleteMatching: {
            std::ostringstream oss;
            for(const auto& l : lines){
                if(l.find(match_pattern) == std::string::npos){
                    oss << l << "\n";
                }
            }
            node->write(oss.str());
            return true;
        }

        case Type::CommentOut: {
            std::ostringstream oss;
            for(const auto& l : lines){
                if(l.find(match_pattern) != std::string::npos){
                    oss << "// " << l << "\n";
                } else {
                    oss << l << "\n";
                }
            }
            node->write(oss.str());
            return true;
        }

        case Type::ReplaceBlock:
            // TODO: Implement block replacement using AST analysis
            return false;
    }

    return false;
}

