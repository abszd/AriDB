// File I/O
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// Standard library
#include <cstdint>      // uint8_t, uint16_t, uint32_t
#include <cstring>      // memset, memcpy
#include <stdexcept>    // std::runtime_error
#include <unordered_map> // hash table

// Optional for debugging
#include <iostream>
#include <cerrno>       // errno, perror()
#include "buffer.h"

typedef std::runtime_error error;

// Calculate which pool index to use based on highest_level
// Pool[i] stores 2^i sets of neighbors (1, 2, 4, 8, 16 sets respectively)
// A node with highest_level needs (highest_level) sets for levels 1 through highest_level
// We find the smallest pool that has enough capacity: 2^i >= highest_level
// For highest_level = 5, we need 5 sets, so we need pool[3] (which has 8 sets >= 5)
// Returns pool index (0 to NUM_LEVELS-1) or 0 if highest_level == 0
uint8_t HNSWBuffer::getPoolIndex(uint8_t highest_level) {
    if (highest_level == 0) {
        return 0; // No pool needed, everything in nodePool
    }
    // Find smallest i where 2^i >= highest_level
    // This is ceil(log2(highest_level))
    // For highest_level=5: need pool where 2^i >= 5, so i=3 (2^3=8 >= 5)
    uint8_t pool_idx = 0;
    uint8_t capacity = 1; // 2^0 = 1
    while (capacity < highest_level && pool_idx < NUM_LEVELS - 1) {
        pool_idx++;
        capacity = 1 << pool_idx; // 2^pool_idx
    }
    return pool_idx;
}

HNSWBuffer::HNSWBuffer(const char* filename, int num_frames){
    numFrames = num_frames; 
    fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        throw error("Failed to open file");
    }
    nodeTable = new FrameDesc[num_frames]; 
    for (int i = 0; i < num_frames; i++)
    {
        nodeTable[i].Clear();
    }
    // Read or create header
    ssize_t bytes = read(fd, &header, sizeof(HNSWHeader));
    if (bytes != sizeof(HNSWHeader)) {
        // New file - create default header
        memset(&header, 0, sizeof(HNSWHeader));
        header.dim = 128;  // Default or pass as parameter
        header.max_level = 4;
        header.max_neighbors = 16;
        header.max_seen_level = 0;
        header.node_count = 0;
        header.entry_node_id = 0;
        
        lseek(fd, 0, SEEK_SET);
        write(fd, &header, sizeof(HNSWHeader));
    }
    levelfd = new int[header.max_level];
    for(int i = 0; i < header.max_seen_level; i++){
        levelfd[i] = open(filename, O_RDWR , 0644);
        if (levelfd[i] < 0) {
            throw error("Failed to open file");
        }
    }

    node_size = sizeof(uint32_t); // level_id
    node_size += header.dim * sizeof(float); // vector
    node_size += sizeof(uint8_t); // highest level in node
    node_size += sizeof(double); // inverse magnitude
    node_size += sizeof(uint8_t); // num neighbors at level 0
    level_size = sizeof(uint32_t) * header.max_neighbors;
    node_size += level_size * 2; // bottom level neighbor ids 
    
    nodePool = new char[node_size * num_frames];

    levelPool = new char*[NUM_LEVELS];
    levelPoolSize = new size_t[NUM_LEVELS]; // Track actual allocated size
    for(int i = 0; i < NUM_LEVELS; i++){ 
        levelPool[i] = nullptr; // Start with null - allocate dynamically when needed
        levelPoolSize[i] = 0;
    }
    clockHand = num_frames - 1;
}

HNSWBuffer::~HNSWBuffer(){
    if (nodePool) delete[] nodePool;
    if (nodeTable) delete[] nodeTable;
    if (levelfd) delete[] levelfd;
    
    // Clean up level pools
    if (levelPool) {
        for(int i = 0; i < NUM_LEVELS; i++) {
            if (levelPool[i]) delete[] levelPool[i];
        }
        delete[] levelPool;
    }
    if (levelPoolSize) delete[] levelPoolSize;
    
    if (fd >= 0) close(fd);
    for(int i = 0; i < header.max_seen_level && levelfd; i++) {
        if (levelfd[i] >= 0) close(levelfd[i]);
    }
}
/**
 * allocate a frame for a new node
 */
int HNSWBuffer::allocFrame()
{
    int checked = 0;
    // if we've checked all frames twice to allow for the reference bit then we're at capacity and checked is being incremented here
    while (checked++ < numFrames * 2)
    {
        // Advance the clock hand. Does this first because clockHand is initialized to numBufs - 1. Advancing it will cause it to wrap around to 0
        advanceClock();
        FrameDesc *curFrame = &nodeTable[clockHand]; // current frame we're checking

        if (!curFrame->valid) // if we have an invalid frame we can just allocoate
        {
            curFrame->Clear();
            return clockHand;
        }

        if (curFrame->refbit) // if the refbit is 1 set to 0
        {
            curFrame->refbit = false;
            continue;
        }

        if (curFrame->pinCnt > 0) // try next frame if frame is pinned
        {
            continue;
        }

        if (curFrame->dirty) // if weve modified the page flush changes
        {
            if (!writeFrame(clockHand))
            {
                throw error("ALLOCFRAME: FAILED TO WRITE FRAME TO DISK");
            }
        }

        if (curFrame->valid) // Safety check for validity
        {
            hashTable.erase(curFrame->node_id);
        }

        // Set frame ptr and clear other variables
        curFrame->Clear();
        return clockHand;
    }
    throw error("ALLOCFRAME: CLOCKHAND REACHED END OF BUFFER"); // If we reach this weve tried every frame and allowed for the reference bit to switch
}

Node* HNSWBuffer::addNode(Node* new_node){
    allocFrame();

}
/**
 * write a frame to disk
 */    

int HNSWBuffer::writeFrame(int frame) {
    uint32_t node_id = nodeTable[frame].node_id; // get node position 
    uint32_t level_id = nodeTable[frame].level_id;
    uint8_t lvl = getPoolIndex(nodeTable[frame].level_id >> 32);
    size_t offset = sizeof(HNSWHeader) + (node_id * node_size); // node offset
    
    char* frame_data = nodePool + (frame * node_size);

    // Seek to position
    if (lseek(fd, offset, SEEK_SET) == -1) {
        throw error("Failed to seek in file");
    }
    
    // Write data
    ssize_t bytes_written = write(fd, frame_data, node_size);
    if (bytes_written != node_size) {
        throw error("Failed to write frame to disk");
    }
    if(lvl == 0){
        return 0;
    }

    int mult = 1;
    int tmp = lvl;
    while(tmp != 1){
        tmp >>= 1;
        mult <<= 1;
    }

    size_t level_offset = level_id * levelPoolSize[lvl-1];
    char * level_data = levelPool[lvl-1] + (level_id * levelPoolSize[lvl-1]);
    
    if (lseek(levelfd[lvl-1], level_offset, SEEK_SET) == -1) {
        throw error("Failed to seek in file");
    }
    bytes_written = write(levelfd[lvl-1], level_data, levelPoolSize[lvl-1]);
    if (bytes_written != levelPoolSize[lvl-1]) {
        throw error("Failed to write frame to disk");
    }
    nodeTable[frame].dirty = false;
}

/**
 * Read a given node_id from file into a given frame
 */
uint32_t HNSWBuffer::readFrame(uint32_t node_id, int frame) {
    size_t offset = sizeof(HNSWHeader) + (node_id * node_size); // node offset
    char* frame_data = nodePool + (frame * node_size);

    if (lseek(fd, offset, SEEK_SET) == -1) {
        throw error("Failed to seek in file");
    }

    ssize_t bytes_read = read(fd, frame_data, node_size);
    if (bytes_read != node_size) {
        throw error("Failed to read frame from disk");
    }

    uint32_t level_id = *(uint32_t *) frame_data; 
    return level_id;
}

Node* HNSWBuffer::getNode(uint32_t node_id){
    //if the node is already loaded, get the frame number and extract it from the nodepool
    if(hashTable.find(node_id) != hashTable.end()){
        int frameno = hashTable[node_id];
        FrameDesc * desc = &nodeTable[frameno];
        
        desc->pinCnt++;
        desc->refbit = true;

        return deserializeNode(frameno);
    }

    //otherwise we need to allocate a frame and load the node from memory
    int frameno = allocFrame();
    uint32_t level_id = readFrame(node_id, frameno);
    
    nodeTable[frameno].Set(node_id);
    nodeTable[frameno].level_id = level_id;
    hashTable[node_id] = frameno;

    return deserializeNode(frameno);
}

void HNSWBuffer::releaseNode(uint32_t node_id, bool dirty){
    if(hashTable.find(node_id) == hashTable.end()){
        throw error("ReleaseNode: unable to find nod_id in hashtable");
    }

    int frameno = hashTable[node_id];
    FrameDesc * frame_data = &nodeTable[frameno];
    if(dirty){
        writeFrame(frameno);
    }

    frame_data->pinCnt--;
}

Node* HNSWBuffer::deserializeNode(int frameno){
    char * node_pool_ptr = nodePool + (frameno * node_size);
    char * level_pool_ptr;
    char * cursor = node_pool_ptr;

    Node* node = new Node(header.dim, header.max_neighbors);
    node->node_id = nodeTable[frameno].node_id;

    // Read level_id (first uint32_t)
    node->level_id = *((uint32_t *) cursor);
    cursor += sizeof(uint32_t);
    
    // Read vector
    node->vector = new float[header.dim];
    memcpy(node->vector, cursor, sizeof(float) * header.dim);
    cursor += sizeof(float) * header.dim;

    node->highest_level = *((uint8_t *) cursor);
    cursor += sizeof(uint8_t);

    node->inv_magnitude = *((double*) cursor);
    cursor += sizeof(double);

    node->num_zneighbors = *((uint8_t*) cursor);
    cursor += sizeof(uint8_t);

    // Read level 0 neighbors (zneighbors) - stored as uint32_t array
    memcpy(node->zneighbors, cursor, sizeof(uint32_t) * header.max_neighbors * 2);
    cursor += sizeof(uint32_t) * header.max_neighbors * 2;
    
    
    if(node->highest_level == 0){
        return node;
    }

    // For nodes with higher levels, load from appropriate level pool
    uint8_t pool_idx = getPoolIndex(node->highest_level);
    uint32_t pool_offset = (uint32_t)(nodeTable[frameno].level_id); // Lower 32 bits
    
    // Calculate size per entry in this pool: 2^pool_idx sets of neighbors + num_neighbors array
    uint8_t sets_in_pool = 1 << pool_idx; // 2^pool_idx sets
    size_t entry_size = (sets_in_pool * level_size) + (sizeof(uint8_t) * node->highest_level);
    
    // Ensure pool is allocated
    if (!levelPool[pool_idx]) {
        // TODO: Allocate pool dynamically when needed
        throw error("Level pool not allocated - need dynamic allocation");
    }
    
    // Get pointer to this node's level data in the pool
    level_pool_ptr = levelPool[pool_idx] + (pool_offset * entry_size);
    
    // Read num_neighbors array (one per level 1 through highest_level)
    node->num_neighbors = new uint8_t[node->highest_level + 1];
    node->num_neighbors[0] = node->num_zneighbors; // Level 0 already read
    memcpy(node->num_neighbors + 1, level_pool_ptr, sizeof(uint8_t) * node->highest_level);
    level_pool_ptr += sizeof(uint8_t) * node->highest_level;
    
    // Allocate and read neighbor arrays for levels 1 to highest_level
    node->neighbors = new uint32_t*[node->highest_level + 1];
    node->neighbors[0] = node->zneighbors; // Level 0 already set
    
    for(int i = 1; i <= node->highest_level; i++){
        node->neighbors[i] = new uint32_t[header.max_neighbors];
        memcpy(node->neighbors[i], level_pool_ptr, level_size);
        level_pool_ptr += level_size;
    }
    
    return node;
}