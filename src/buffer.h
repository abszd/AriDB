#include <unordered_map>
#include <cstdint>
#define NUM_LEVELS 5

// Forward declarations
struct Node;
struct NodeDesc;
struct HNSWHeader; 

class HNSWBuffer {
private:
    int numFrames; // How many nodes fit in memory
    size_t node_size; // Bytes per node (calculated from header)
    size_t level_size;
    int clockHand; // For clock eviction
    HNSWHeader header;
    
    int fd; // File descriptor for hnsw.bin
    int *levelfd;
    uint8_t maxSeenH;
    // Buffer pool
    char* nodePool; // Raw bytes: frames * node_size
    char** levelPool; 
    size_t* levelPoolSize; // Current size in bytes of each pool (for dynamic resizing)
    FrameDesc* nodeTable; // Metadata per frame
    std::unordered_map<uint32_t, int> hashTable; // node_id → frame
    
    // Helpers
    int allocFrame(); // Find free frame (clock algorithm)
    void advanceClock()
    {
        clockHand = (clockHand + 1) % numFrames;
    }

    // Calculate which pool a node's levels belong to based on highest_level
    // Pool[i] stores 2^i sets of neighbors (1, 2, 4, 8, 16 sets respectively)
    // A node with highest_level needs (highest_level) sets for levels 1 through highest_level
    // Returns the pool index (0 to NUM_LEVELS-1)
    // For highest_level=5: need pool where 2^i >= 5, so pool_index = 3 (pool[3] has 8 sets)
    uint8_t getPoolIndex(uint8_t highest_level);
    
    Node* deserializeNode(int frame);  // Parse bytes → Node struct
    int serializeNode(Node* node, int frame);  // Node struct → bytes
    int writeFrame(int frame); // Write frame to disk
    uint32_t readFrame(uint32_t node_id, int frame); // Read from disk
    int writeLevel(int frame);
    
public:
    HNSWBuffer(const char* filename, int num_frames);
    ~HNSWBuffer();
    
    // Main API
    Node* getNode(uint32_t node_id);              // Load node (pin it)
    void releaseNode(uint32_t node_id, bool dirty); // Unpin node
    void flush();                                   // Write all dirty
};

struct HNSWHeader{
    uint16_t dim;               // 2 bytes
    uint8_t max_level;          // 1 byte
    uint8_t max_neighbors;      // 1 byte
    uint8_t max_seen_level;
    uint32_t node_count;        // 8 bytes
    uint32_t entry_node_id;     // 8 bytes
    uint8_t reserved[44];       // 44 bytes
};

// Simple frame metadata
struct FrameDesc {
    uint32_t node_id;
    uint64_t level_id;  // Upper 32 bits: pool index, Lower 32 bits: offset within pool
    bool valid;
    bool dirty;
    int pinCnt;
    bool refbit;
    
    void Clear() {  // initialize buffer frame for a new user
        node_id = 0;
        pinCnt = 0;
        dirty = false;
        refbit = false;
        valid = false;
    };

    void Set(uint32_t id) { 
        node_id = id;
        pinCnt = 1;
        dirty = false;
        valid = true;
        refbit = true;
    }

    FrameDesc() {
        Clear();
    }
};


struct Node {
    uint32_t level_id;
    float* vector;              // Pointer to vector data (dim floats)
    uint8_t highest_level;      // Max level this node appears in
    double inv_magnitude;       // 1.0 / ||vector|| for cosine similarity
    uint8_t num_zneighbors;     // number of neighbors on 0 level
    uint32_t* zneighbors;    // neighbors on 0 level
    uint8_t* num_neighbors;   
    uint32_t** neighbors;

    uint32_t node_id;           // Node's ID
    Node(){}

    Node(uint16_t d, uint8_t mn) {
        vector = new float[d];
        num_neighbors = new uint8_t[highest_level + 1];
        neighbors = new uint32_t*[highest_level + 1];
        num_zneighbors = 0;
        zneighbors = new uint32_t[mn * 2];
        for (int i = 1; i <= highest_level; i++) {
            num_neighbors[i] = 0;
            neighbors[i] = new uint32_t[mn];
        }
    }
    
    ~Node() {
        delete[] vector;
        delete[] num_neighbors;
        for (int i = 0; i <= highest_level; i++) {
            delete[] neighbors[i];
        }
        delete[] neighbors;
    }
};