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

HNSWBuffer::HNSWBuffer(const char* filename, int num_frames){
    numFrames = num_frames; 
    fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        throw error("Failed to open file");
    }
    nodeTable = new NodeDesc[num_frames]; 
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
        header.node_count = 0;
        header.entry_node_id = 0;
        
        lseek(fd, 0, SEEK_SET);
        write(fd, &header, sizeof(HNSWHeader));
    }
    
    node_size = header.dim * sizeof(float); // vector
    node_size += sizeof(uint8_t); // highest level in node
    node_size += sizeof(double); // inverse magnitude
    node_size += (header.max_level + 1) * sizeof(uint8_t); // num neighbors per level
    node_size += (header.max_level * header.max_neighbors) * sizeof(uint32_t); // neighbor ids  
    node_size += header.max_neighbors * 2 * sizeof(uint32_t); // bottom level neighbor ids 
    
    nodePool = new char[node_size * num_frames];

    clockHand = num_frames - 1;
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
        NodeDesc *curFrame = &nodeTable[clockHand]; // current frame we're checking

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

/**
 * write a frame to disk
 */
int HNSWBuffer::writeFrame(int frame) {
    uint32_t node_id = nodeTable[frame].node_id; // get node position 
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
    
    nodeTable[frame].dirty = false;
}

/**
 * Read a given node_id from file into a given frame
 */
int HNSWBuffer::readFrame(uint32_t node_id, int frame) {
    size_t offset = sizeof(HNSWHeader) + (node_id * node_size); // node offset
    char* frame_data = nodePool + (frame * node_size);

    if (lseek(fd, offset, SEEK_SET) == -1) {
        throw error("Failed to seek in file");
    }

    ssize_t bytes_read = read(fd, frame_data, node_size);
    if (bytes_read != node_size) {
        throw error("Failed to read frame from disk");
    }

    return 0;
}


Node* HNSWBuffer::getNode(uint32_t node_id){
    //if the node is already loaded, get the frame number and extract it from the nodepool
    if(hashTable.find(node_id) != hashTable.end()){
        int frameno = hashTable[node_id];
        NodeDesc * desc = &nodeTable[frameno];
        
        desc->pinCnt++;
        desc->refbit = true;

        return deserializeNode(frameno);
    }

    //otherwise we need to allocate a frame and load the node from memory
    int frameno = allocFrame();
    readFrame(node_id, frameno);
    
    nodeTable[frameno].Set(node_id);
    hashTable[node_id] = frameno;

    return deserializeNode(frameno);
}

Node* HNSWBuffer::deserializeNode(int frameno){
    char * node_pool_ptr = nodePool + (frameno * node_size);
    char * level_pool_ptr;// level_pool + frameno * level_size 
    char * cursor = node_pool_ptr;

    Node* node = new Node(header.dim, 0, header.max_neighbors);
    node->node_id = nodeTable[frameno].node_id;

    memcpy(node->vector, cursor, sizeof(float) * node->dim);
    cursor += sizeof(float) * node->dim;

    node->highest_level = *((uint8_t *) cursor);
    cursor += sizeof(uint8_t);

    node->inv_magnitude = *((double*) cursor);
    cursor += sizeof(double);

    memcpy(node->num_neighbors, cursor, sizeof(uint8_t) * header.max_level);
    cursor += sizeof(uint8_t) * header.max_level;

    
}