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
    std::unordered_map<uint32_t, int> hashTable; // node_id → frame
    
    // Helpers
    int allocFrame(); // Find free frame (clock algorithm)
    void advanceClock()
    {
        clockHand = (clockHand + 1) % numFrames;
    }

    Node* deserializeNode(int frame);  // Parse bytes → Node struct
    int serializeNode(Node* node, int frame);  // Node struct → bytes
    int writeFrame(int frame); // Write frame to disk
    int readFrame(uint32_t node_id, int frame); // Read from disk
    
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
    uint32_t node_count;        // 8 bytes
    uint32_t entry_node_id;     // 8 bytes
    uint8_t reserved[44];       // 44 bytes
};

// Simple frame metadata
struct NodeDesc {
    uint32_t node_id;
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

    NodeDesc() {
        Clear();
    }
};

struct Node {
    float* vector;              // Pointer to vector data (dim floats)
    uint8_t highest_level;      // Max level this node appears in
    double inv_magnitude;       // 1.0 / ||vector|| for cosine similarity
    uint8_t num_zneighbors;     // number of neighbors on 0 level
    uint32_t zneighbors[32];    // neighbors on 0 level
    uint8_t* num_neighbors;     // Array of neighbor counts per level [highest level]
    uint32_t** neighbors;       // 2D array: neighbors[level][neighbor_idx]
    
    uint32_t node_id;           // Node's ID

    Node(uint16_t d, uint8_t mn) {
        vector = new float[d];
        num_neighbors = new uint8_t[highest_level + 1];
        neighbors = new uint32_t*[highest_level + 1];
        for (int i = 0; i <= highest_level; i++) {
            num_neighbors[i] = 0;
            neighbors[i] = new uint32_t[i == 0 ? mn * 2 : mn];
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