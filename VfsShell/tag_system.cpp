#include "VfsShell.h"

// ====== Tag Registry & Storage ======
TagId TagRegistry::registerTag(const std::string& name){
    auto it = name_to_id.find(name);
    if(it != name_to_id.end()) return it->second;
    TagId id = next_id++;
    name_to_id[name] = id;
    id_to_name[id] = name;
    return id;
}

TagId TagRegistry::getTagId(const std::string& name) const {
    auto it = name_to_id.find(name);
    return it != name_to_id.end() ? it->second : TAG_INVALID;
}

std::string TagRegistry::getTagName(TagId id) const {
    auto it = id_to_name.find(id);
    return it != id_to_name.end() ? it->second : "";
}

bool TagRegistry::hasTag(const std::string& name) const {
    return name_to_id.find(name) != name_to_id.end();
}

std::vector<std::string> TagRegistry::allTags() const {
    std::vector<std::string> tags;
    tags.reserve(name_to_id.size());
    for(const auto& pair : name_to_id){
        tags.push_back(pair.first);
    }
    return tags;
}

void TagStorage::addTag(VfsNode* node, TagId tag){
    if(!node || tag == TAG_INVALID) return;
    node_tags[node].insert(tag);
}

void TagStorage::removeTag(VfsNode* node, TagId tag){
    if(!node) return;
    auto it = node_tags.find(node);
    if(it != node_tags.end()){
        it->second.erase(tag);
        if(it->second.empty()) node_tags.erase(it);
    }
}

bool TagStorage::hasTag(VfsNode* node, TagId tag) const {
    if(!node) return false;
    auto it = node_tags.find(node);
    if(it == node_tags.end()) return false;
    return it->second.count(tag) > 0;
}

const TagSet* TagStorage::getTags(VfsNode* node) const {
    if(!node) return nullptr;
    auto it = node_tags.find(node);
    return it != node_tags.end() ? &it->second : nullptr;
}

void TagStorage::clearTags(VfsNode* node){
    if(node) node_tags.erase(node);
}

std::vector<VfsNode*> TagStorage::findByTag(TagId tag) const {
    std::vector<VfsNode*> result;
    for(const auto& pair : node_tags){
        if(pair.second.count(tag) > 0){
            result.push_back(pair.first);
        }
    }
    return result;
}

std::vector<VfsNode*> TagStorage::findByTags(const TagSet& tags, bool match_all) const {
    std::vector<VfsNode*> result;
    for(const auto& pair : node_tags){
        if(match_all){
            // All tags must be present
            bool has_all = true;
            for(TagId tag : tags){
                if(pair.second.count(tag) == 0){
                    has_all = false;
                    break;
                }
            }
            if(has_all) result.push_back(pair.first);
        } else {
            // Any tag can be present
            bool has_any = false;
            for(TagId tag : tags){
                if(pair.second.count(tag) > 0){
                    has_any = true;
                    break;
                }
            }
            if(has_any) result.push_back(pair.first);
        }
    }
    return result;
}

// TagMiningSession implementation
void TagMiningSession::addUserTag(TagId tag){
    user_provided_tags.insert(tag);
}

void TagMiningSession::recordFeedback(const std::string& tag_name, bool confirmed){
    user_feedback[tag_name] = confirmed;
}

// VFS tag helpers
