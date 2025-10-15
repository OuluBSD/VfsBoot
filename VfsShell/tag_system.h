#pragma once


// Tag Registry (enumerated tags for memory efficiency)
//
using TagId = uint32_t;
constexpr TagId TAG_INVALID = 0;

//
// BitVector-based TagSet for O(1) operations with 64-bit chunks
//
// Simple BitVector class for feature masks
class BitVector {
    static constexpr size_t BITS_PER_CHUNK = 64;
    std::vector<uint64_t> chunks;
    size_t num_bits;

    static constexpr size_t chunkIndex(size_t bit) { return bit / BITS_PER_CHUNK; }
    static constexpr size_t bitOffset(size_t bit) { return bit % BITS_PER_CHUNK; }

public:
    explicit BitVector(size_t bits = 512) : num_bits(bits) {
        chunks.resize((bits + BITS_PER_CHUNK - 1) / BITS_PER_CHUNK, 0);
    }

    void set(size_t bit) {
        if (bit < num_bits) {
            size_t idx = chunkIndex(bit);
            if (idx >= chunks.size()) chunks.resize(idx + 1, 0);
            chunks[idx] |= (1ULL << bitOffset(bit));
        }
    }

    void clear(size_t bit) {
        if (bit < num_bits && bit < chunks.size() * BITS_PER_CHUNK) {
            chunks[chunkIndex(bit)] &= ~(1ULL << bitOffset(bit));
        }
    }

    bool test(size_t bit) const {
        if (bit >= num_bits) return false;
        size_t idx = chunkIndex(bit);
        return idx < chunks.size() && (chunks[idx] & (1ULL << bitOffset(bit)));
    }

    uint64_t hash() const {
        uint64_t h = 0;
        for (auto chunk : chunks) h ^= chunk;
        return h;
    }

    std::string toString() const {
        std::ostringstream out;
        out << std::hex;
        for (size_t i = 0; i < chunks.size(); i++) {
            if (i > 0) out << ":";
            out << chunks[i];
        }
        return out.str();
    }

    static BitVector fromString(const std::string& s) {
        BitVector bv(512);
        std::istringstream in(s);
        in >> std::hex;
        size_t idx = 0;
        std::string chunk_str;
        while (std::getline(in, chunk_str, ':')) {
            if (idx >= bv.chunks.size()) bv.chunks.resize(idx + 1, 0);
            bv.chunks[idx++] = std::stoull(chunk_str, nullptr, 16);
        }
        return bv;
    }
};

class TagSet {
    static constexpr size_t BITS_PER_CHUNK = 64;
    std::vector<uint64_t> chunks;  // Bit vector in 64-bit chunks

    // Get chunk index and bit position for a tag
    static constexpr size_t chunkIndex(TagId tag) { return tag / BITS_PER_CHUNK; }
    static constexpr size_t bitPosition(TagId tag) { return tag % BITS_PER_CHUNK; }

    void ensureCapacity(TagId tag) {
        size_t needed = chunkIndex(tag) + 1;
        if(chunks.size() < needed) {
            chunks.resize(needed, 0);
        }
    }

public:
    TagSet() = default;
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
        size_t idx = chunkIndex(tag);
        if(idx < chunks.size()) {
            chunks[idx] &= ~(1ULL << bitPosition(tag));
        }
    }

    // Check membership - O(1)
    size_t count(TagId tag) const {
        if(tag == TAG_INVALID) return 0;
        size_t idx = chunkIndex(tag);
        if(idx >= chunks.size()) return 0;
        return (chunks[idx] & (1ULL << bitPosition(tag))) ? 1 : 0;
    }

    bool contains(TagId tag) const { return count(tag) > 0; }

    // Cardinality using popcount - O(chunks)
    size_t size() const {
        size_t total = 0;
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

    void clear() { chunks.clear(); }

    // Set operations using bitwise ops
    TagSet operator|(const TagSet& other) const {  // Union
        TagSet result;
        size_t max_size = std::max(chunks.size(), other.chunks.size());
        result.chunks.resize(max_size, 0);
        for(size_t i = 0; i < chunks.size(); ++i) {
            result.chunks[i] = chunks[i];
        }
        for(size_t i = 0; i < other.chunks.size(); ++i) {
            result.chunks[i] |= other.chunks[i];
        }
        return result;
    }

    TagSet operator&(const TagSet& other) const {  // Intersection
        TagSet result;
        size_t min_size = std::min(chunks.size(), other.chunks.size());
        result.chunks.resize(min_size, 0);
        for(size_t i = 0; i < min_size; ++i) {
            result.chunks[i] = chunks[i] & other.chunks[i];
        }
        return result;
    }

    TagSet operator-(const TagSet& other) const {  // Difference
        TagSet result;
        result.chunks.resize(chunks.size(), 0);
        for(size_t i = 0; i < chunks.size(); ++i) {
            uint64_t other_chunk = i < other.chunks.size() ? other.chunks[i] : 0;
            result.chunks[i] = chunks[i] & ~other_chunk;
        }
        return result;
    }

    TagSet operator^(const TagSet& other) const {  // Symmetric difference (XOR)
        TagSet result;
        size_t max_size = std::max(chunks.size(), other.chunks.size());
        result.chunks.resize(max_size, 0);
        for(size_t i = 0; i < max_size; ++i) {
            uint64_t a = i < chunks.size() ? chunks[i] : 0;
            uint64_t b = i < other.chunks.size() ? other.chunks[i] : 0;
            result.chunks[i] = a ^ b;
        }
        return result;
    }

    // In-place operations
    TagSet& operator|=(const TagSet& other) {
        if(other.chunks.size() > chunks.size()) {
            chunks.resize(other.chunks.size(), 0);
        }
        for(size_t i = 0; i < other.chunks.size(); ++i) {
            chunks[i] |= other.chunks[i];
        }
        return *this;
    }

    TagSet& operator&=(const TagSet& other) {
        for(size_t i = 0; i < chunks.size(); ++i) {
            chunks[i] &= i < other.chunks.size() ? other.chunks[i] : 0;
        }
        return *this;
    }

    TagSet& operator-=(const TagSet& other) {
        for(size_t i = 0; i < std::min(chunks.size(), other.chunks.size()); ++i) {
            chunks[i] &= ~other.chunks[i];
        }
        return *this;
    }

    // Fast subset checking
    bool isSubsetOf(const TagSet& other) const {
        for(size_t i = 0; i < chunks.size(); ++i) {
            uint64_t other_chunk = i < other.chunks.size() ? other.chunks[i] : 0;
            if((chunks[i] & ~other_chunk) != 0) return false;
        }
        return true;
    }

    bool isSupersetOf(const TagSet& other) const {
        return other.isSubsetOf(*this);
    }

    // Iterator support for range-based for loops
    class iterator {
        const std::vector<uint64_t>* chunks_ptr;
        size_t chunk_idx;
        size_t bit_idx;

        void advance() {
            if(!chunks_ptr || chunk_idx >= chunks_ptr->size()) return;

            // Find next set bit
            while(chunk_idx < chunks_ptr->size()) {
                uint64_t chunk = (*chunks_ptr)[chunk_idx];
                if(chunk == 0) {
                    chunk_idx++;
                    bit_idx = 0;
                    continue;
                }

                // Skip to next set bit in current chunk
                chunk >>= bit_idx;
                while(bit_idx < BITS_PER_CHUNK && (chunk & 1) == 0) {
                    chunk >>= 1;
                    bit_idx++;
                }

                if(bit_idx < BITS_PER_CHUNK) return;  // Found
                chunk_idx++;
                bit_idx = 0;
            }
        }

    public:
        iterator(const std::vector<uint64_t>* ptr, bool is_end)
            : chunks_ptr(ptr), chunk_idx(is_end ? ptr->size() : 0), bit_idx(0) {
            if(!is_end && ptr && !ptr->empty()) advance();
        }

        TagId operator*() const {
            return static_cast<TagId>(chunk_idx * BITS_PER_CHUNK + bit_idx);
        }

        iterator& operator++() {
            bit_idx++;
            advance();
            return *this;
        }

        bool operator!=(const iterator& other) const {
            return chunk_idx != other.chunk_idx || bit_idx != other.bit_idx;
        }

        bool operator==(const iterator& other) const {
            return !(*this != other);
        }
    };

    iterator begin() const { return iterator(&chunks, false); }
    iterator end() const { return iterator(&chunks, true); }

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
        size_t max_size = std::max(chunks.size(), other.chunks.size());
        for(size_t i = 0; i < max_size; ++i) {
            uint64_t a = i < chunks.size() ? chunks[i] : 0;
            uint64_t b = i < other.chunks.size() ? other.chunks[i] : 0;
            if(a != b) return false;
        }
        return true;
    }

    bool operator!=(const TagSet& other) const {
        return !(*this == other);
    }
};

struct TagRegistry {
    std::map<std::string, TagId> name_to_id;
    std::map<TagId, std::string> id_to_name;
    TagId next_id = 1;

    TagId registerTag(const std::string& name);
    TagId getTagId(const std::string& name) const;
    std::string getTagName(TagId id) const;
    bool hasTag(const std::string& name) const;
    std::vector<std::string> allTags() const;
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
    std::vector<VfsNode*> findByTag(TagId tag) const;
    std::vector<VfsNode*> findByTags(const TagSet& tags, bool match_all) const;
};

// Tag Mining Session
struct TagMiningSession {
    TagSet user_provided_tags;
    TagSet inferred_tags;
    std::vector<std::string> pending_questions;  // questions to ask user
    std::map<std::string, bool> user_feedback;   // tag_name -> confirmed

    void addUserTag(TagId tag);
    void recordFeedback(const std::string& tag_name, bool confirmed);
};
