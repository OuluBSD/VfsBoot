#include "Logic.h"
#include "../VfsShell/VfsShell.h"

// ====== Tag Registry & Storage ======
TagId TagRegistry::registerTag(const String& name){
    int idx = name_to_id.Find(name);
    if(idx >= 0) return name_to_id[idx];
    TagId id = next_id++;
    name_to_id.Add(name, id);
    id_to_name.Add(id, name);
    return id;
}

TagId TagRegistry::getTagId(const String& name) const {
    int idx = name_to_id.Find(name);
    return idx >= 0 ? name_to_id[idx] : TAG_INVALID;
}

String TagRegistry::getTagName(TagId id) const {
    int idx = id_to_name.Find(id);
    return idx >= 0 ? id_to_name[idx] : String();
}

bool TagRegistry::hasTag(const String& name) const {
    return name_to_id.Find(name) >= 0;
}

Vector<String> TagRegistry::allTags() const {
    Vector<String> tags;
    tags.SetCount(name_to_id.GetCount());
    for(int i = 0; i < name_to_id.GetCount(); i++){
        tags[i] = name_to_id.GetKey(i);
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

Vector<VfsNode*> TagStorage::findByTag(TagId tag) const {
    Vector<VfsNode*> result;
    for(const auto& pair : node_tags){
        if(pair.second.count(tag) > 0){
            result.Add(pair.first);
        }
    }
    return result;
}

Vector<VfsNode*> TagStorage::findByTags(const TagSet& tags, bool match_all) const {
    Vector<VfsNode*> result;
    Vector<TagId> tag_vec = tags.toVector();
    for(const auto& pair : node_tags){
        if(match_all){
            // All tags must be present
            bool has_all = true;
            for(int i = 0; i < tag_vec.GetCount(); i++){
                if(pair.second.count(tag_vec[i]) == 0){
                    has_all = false;
                    break;
                }
            }
            if(has_all) result.Add(pair.first);
        } else {
            // Any tag can be present
            bool has_any = false;
            for(int i = 0; i < tag_vec.GetCount(); i++){
                if(pair.second.count(tag_vec[i]) > 0){
                    has_any = true;
                    break;
                }
            }
            if(has_any) result.Add(pair.first);
        }
    }
    return result;
}

// TagMiningSession implementation
void TagMiningSession::addUserTag(TagId tag){
    user_provided_tags.insert(tag);
}

void TagMiningSession::recordFeedback(const String& tag_name, bool confirmed){
    int idx = user_feedback.Find(tag_name);
    if(idx >= 0){
        user_feedback[idx] = confirmed;
    } else {
        user_feedback.Add(tag_name, confirmed);
    }
}

// VFS tag helpers
