#include <unordered_map>
#include <cstdint>

// Forward declarations
struct Node;
struct NodeDesc;
struct HNSWHeader; 

class HNSWBuffer {
private:
    int numFrames; // How many nodes fit in memory
    size_t node_size; // Bytes per node (calculated from header)
    int clockHand; // For clock eviction
    HNSWHeader header;
    
    int fd; // File descriptor for hnsw.bin
    
    // Buffer pool
    char* nodePool; // Raw bytes: frames * node_size
    NodeDesc* nodeTable; // Metadata per frame
    std::unordered_map<uint64_t, int> hashTable; // node_id → frame
    
    // Helpers
    int allocFrame(); // Find free frame (clock algorithm)
    void advanceClock()
    {
        clockHand = (clockHand + 1) % numFrames;
    }

    Node* deserializeNode(int frame);  // Parse bytes → Node struct
    int serializeNode(Node* node, int frame);  // Node struct → bytes
    int writeFrame(int frame); // Write frame to disk
    int readFrame(uint64_t node_id, int frame); // Read from disk
    
public:
    HNSWBuffer(const char* filename, int num_frames);
    ~HNSWBuffer();
    
    // Main API
    Node* getNode(uint64_t node_id);              // Load node (pin it)
    void releaseNode(uint64_t node_id, bool dirty); // Unpin node
    void flush();                                   // Write all dirty
};

struct HNSWHeader{
    uint16_t dim;               // 2 bytes
    uint8_t max_level;          // 1 byte
    uint8_t max_neighbors;      // 1 byte
    uint64_t node_count;        // 8 bytes
    uint64_t entry_node_id;     // 8 bytes
    uint8_t reserved[44];       // 44 bytes
};

// Simple frame metadata
struct NodeDesc {
    uint64_t node_id;
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

    void Set(uint64_t id) { 
        node_id = id;
        pinCnt = 1;
        dirty = false;
        valid = true;
        refbit = true;
    }

    NodeDesc() {
        Clear();
    }
};
