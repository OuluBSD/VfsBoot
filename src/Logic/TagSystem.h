#ifndef _Logic_TagSystem_h_
#define _Logic_TagSystem_h_

// All includes have been moved to Logic.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs types from VfsCore.h
// #include "../VfsCore/VfsCore.h"  // Commented for U++ convention - included in main header

// Forward declarations for types in other packages
struct VfsNode;
using TagId = size_t;

// Tag Registry (enumerated tags for memory efficiency)
//
//using TagId = uint32_t;  // Already defined above
constexpr TagId TAG_INVALID = 0;

//
// BitVector-based TagSet for O(1) operations with 64-bit chunks
//
// Simple BitVector class for feature masks
class BitVector {
    static constexpr size_t BITS_PER_CHUNK = 64;
    Vector<uint64_t> chunks;
    int num_bits;

    static constexpr int chunkIndex(int bit) { return bit / BITS_PER_CHUNK; }
    static constexpr int bitOffset(int bit) { return bit % BITS_PER_CHUNK; }

public:
    explicit BitVector(int bits = 512) : num_bits(bits) {
        chunks.SetCount((bits + BITS_PER_CHUNK - 1) / BITS_PER_CHUNK, 0);
    }

    // Copy and move constructors for U++ Vector compatibility
    BitVector(const BitVector& other) : chunks(other.chunks, 1), num_bits(other.num_bits) {}
    BitVector(BitVector&& other) : chunks(pick(other.chunks)), num_bits(other.num_bits) {}
    BitVector& operator=(const BitVector& other) {
        if(this != &other) {
            chunks = clone(other.chunks);
            num_bits = other.num_bits;
        }
        return *this;
    }
    BitVector& operator=(BitVector&& other) {
        if(this != &other) {
            chunks = pick(other.chunks);
            num_bits = other.num_bits;
        }
        return *this;
    }

    void set(int bit) {
        if (bit < num_bits) {
            int idx = chunkIndex(bit);
            if (idx >= chunks.GetCount()) chunks.SetCount(idx + 1, 0);
            chunks[idx] |= (1ULL << bitOffset(bit));
        }
    }

    void clear(int bit) {
        if (bit < num_bits && bit < chunks.GetCount() * BITS_PER_CHUNK) {
            chunks[chunkIndex(bit)] &= ~(1ULL << bitOffset(bit));
        }
    }

    bool test(int bit) const {
        if (bit >= num_bits) return false;
        int idx = chunkIndex(bit);
        return idx < chunks.GetCount() && (chunks[idx] & (1ULL << bitOffset(bit)));
    }

    uint64_t hash() const {
        uint64_t h = 0;
        for (const auto& chunk : chunks) h ^= chunk;
        return h;
    }

    String toString() const {
        String out;
        for (int i = 0; i < chunks.GetCount(); i++) {
            if (i > 0) out << ":";
            out << FormatIntHex(chunks[i]);
        }
        return out;
    }

    static BitVector fromString(const String& s) {
        BitVector bv(512);
        // Simplified implementation - in real code, parse hex string
        // This is just to show the conversion
        return bv;
    }
};

class TagSet {
    static constexpr size_t BITS_PER_CHUNK = 64;

    // Get chunk index and bit position for a tag
    static constexpr int chunkIndex(TagId tag) { return static_cast<int>(tag / BITS_PER_CHUNK); }
    static constexpr int bitPosition(TagId tag) { return static_cast<int>(tag % BITS_PER_CHUNK); }

    void ensureCapacity(TagId tag) {
        int needed = chunkIndex(tag) + 1;
        if(chunks.GetCount() < needed) {
            chunks.SetCount(needed, 0);
        }
    }

public:
    Vector<uint64_t> chunks;  // Bit vector in 64-bit chunks - public for copying

    TagSet() = default;
    TagSet(const TagSet& other) : chunks(other.chunks, 1) {}  // Deep copy using U++ copy
    TagSet(TagSet&& other) : chunks(pick(other.chunks)) {}    // Move
    TagSet& operator=(const TagSet& other) {
        if(this != &other) {
            chunks = clone(other.chunks);
        }
        return *this;
    }
    TagSet& operator=(TagSet&& other) {
        chunks = pick(other.chunks);
        return *this;
    }
    TagSet(std::initializer_list<TagId> tags) {
        for(TagId tag : tags) insert(tag);
    }

    // Insert tag - O(1) amortized
    void insert(TagId tag) {
        if(tag == TAG_INVALID) return;
        ensureCapacity(tag);
        chunks[chunkIndex(tag)] |= (1ULL << bitPosition(tag));
    }

    // Erase tag - O(1)
    void erase(TagId tag) {
        if(tag == TAG_INVALID) return;
        int idx = chunkIndex(tag);
        if(idx < chunks.GetCount()) {
            chunks[idx] &= ~(1ULL << bitPosition(tag));
        }
    }

    // Check membership - O(1)
    int count(TagId tag) const {
        if(tag == TAG_INVALID) return 0;
        int idx = chunkIndex(tag);
        if(idx >= chunks.GetCount()) return 0;
        return (chunks[idx] & (1ULL << bitPosition(tag))) ? 1 : 0;
    }

    bool contains(TagId tag) const { return count(tag) > 0; }

    // Cardinality using popcount - O(chunks)
    int size() const {
        int total = 0;
        for(uint64_t chunk : chunks) {
            total += __builtin_popcountll(chunk);
        }
        return total;
    }

    bool empty() const {
        for(uint64_t chunk : chunks) {
            if(chunk != 0) return false;
        }
        return true;
    }

    void clear() { chunks.Clear(); }

    // Convert to vector for iteration
    Vector<TagId> toVector() const {
        Vector<TagId> result;
        for(int chunk_idx = 0; chunk_idx < chunks.GetCount(); ++chunk_idx) {
            uint64_t chunk = chunks[chunk_idx];
            if(chunk == 0) continue;
            for(int bit = 0; bit < BITS_PER_CHUNK; ++bit) {
                if(chunk & (1ULL << bit)) {
                    result.Add(static_cast<TagId>(chunk_idx * BITS_PER_CHUNK + bit));
                }
            }
        }
        return result;
    }

    // Set operations using bitwise ops
    TagSet operator|(const TagSet& other) const {  // Union
        TagSet result;
        int max_size = std::max(chunks.GetCount(), other.chunks.GetCount());
        result.chunks.SetCount(max_size, 0);
        for(int i = 0; i < chunks.GetCount(); ++i) {
            result.chunks[i] = chunks[i];
        }
        for(int i = 0; i < other.chunks.GetCount(); ++i) {
            result.chunks[i] |= other.chunks[i];
        }
        return result;
    }

    TagSet operator&(const TagSet& other) const {  // Intersection
        TagSet result;
        int min_size = std::min(chunks.GetCount(), other.chunks.GetCount());
        result.chunks.SetCount(min_size, 0);
        for(int i = 0; i < min_size; ++i) {
            result.chunks[i] = chunks[i] & other.chunks[i];
        }
        return result;
    }

    TagSet operator-(const TagSet& other) const {  // Difference
        TagSet result;
        result.chunks.SetCount(chunks.GetCount(), 0);
        for(int i = 0; i < chunks.GetCount(); ++i) {
            uint64_t other_chunk = i < other.chunks.GetCount() ? other.chunks[i] : 0;
            result.chunks[i] = chunks[i] & ~other_chunk;
        }
        return result;
    }

    TagSet operator^(const TagSet& other) const {  // Symmetric difference (XOR)
        TagSet result;
        int max_size = std::max(chunks.GetCount(), other.chunks.GetCount());
        result.chunks.SetCount(max_size, 0);
        for(int i = 0; i < max_size; ++i) {
            uint64_t a = i < chunks.GetCount() ? chunks[i] : 0;
            uint64_t b = i < other.chunks.GetCount() ? other.chunks[i] : 0;
            result.chunks[i] = a ^ b;
        }
        return result;
    }

    // In-place operations
    TagSet& operator|=(const TagSet& other) {
        if(other.chunks.GetCount() > chunks.GetCount()) {
            chunks.SetCount(other.chunks.GetCount(), 0);
        }
        for(int i = 0; i < other.chunks.GetCount(); ++i) {
            chunks[i] |= other.chunks[i];
        }
        return *this;
    }

    TagSet& operator&=(const TagSet& other) {
        for(int i = 0; i < chunks.GetCount(); ++i) {
            chunks[i] &= i < other.chunks.GetCount() ? other.chunks[i] : 0;
        }
        return *this;
    }

    TagSet& operator-=(const TagSet& other) {
        for(int i = 0; i < std::min(chunks.GetCount(), other.chunks.GetCount()); ++i) {
            chunks[i] &= ~other.chunks[i];
        }
        return *this;
    }

    // Fast subset checking
    bool isSubsetOf(const TagSet& other) const {
        for(int i = 0; i < chunks.GetCount(); ++i) {
            uint64_t other_chunk = i < other.chunks.GetCount() ? other.chunks[i] : 0;
            if((chunks[i] & ~other_chunk) != 0) return false;
        }
        return true;
    }

    bool isSupersetOf(const TagSet& other) const {
        return other.isSubsetOf(*this);
    }

    // XOR-based hash for fast fingerprinting
    uint64_t hash() const {
        uint64_t h = 0;
        for(uint64_t chunk : chunks) {
            h ^= chunk;
        }
        return h;
    }

    // Comparison operators
    bool operator==(const TagSet& other) const {
        int max_size = std::max(chunks.GetCount(), other.chunks.GetCount());
        for(int i = 0; i < max_size; ++i) {
            uint64_t a = i < chunks.GetCount() ? chunks[i] : 0;
            uint64_t b = i < other.chunks.GetCount() ? other.chunks[i] : 0;
            if(a != b) return false;
        }
        return true;
    }

    bool operator!=(const TagSet& other) const {
        return !(*this == other);
    }
};

struct TagRegistry {
    VectorMap<String, TagId> name_to_id;
    VectorMap<TagId, String> id_to_name;
    TagId next_id = 1;

    TagId registerTag(const String& name);
    TagId getTagId(const String& name) const;
    String getTagName(TagId id) const;
    bool hasTag(const String& name) const;
    Vector<String> allTags() const;
    typedef TagRegistry CLASSNAME;  // Required for THISBACK macros if used
};
//
// Tag Storage (must be declared after VfsNode)
//
struct TagStorage {
    std::unordered_map<VfsNode*, TagSet> node_tags;

    void addTag(VfsNode* node, TagId tag);
    void removeTag(VfsNode* node, TagId tag);
    bool hasTag(VfsNode* node, TagId tag) const;
    const TagSet* getTags(VfsNode* node) const;
    void clearTags(VfsNode* node);
    Vector<VfsNode*> findByTag(TagId tag) const;
    Vector<VfsNode*> findByTags(const TagSet& tags, bool match_all) const;
};

// Tag Mining Session
struct TagMiningSession {
    TagSet user_provided_tags;
    TagSet inferred_tags;
    Vector<String> pending_questions;  // questions to ask user
    VectorMap<String, bool> user_feedback;   // tag_name -> confirmed

    void addUserTag(TagId tag);
    void recordFeedback(const String& tag_name, bool confirmed);
};

#endif
