#include <iostream>
#include <filesystem>

class BTree {
    // TODO: add internal structure (nodes, root, etc)

    BTree(uint8_t order){

    }  // order = max keys per node
    
    // Core operations
    void insert(size_t id, size_t offset);
    size_t find(size_t id);  // returns offset, throws if not found
    bool exists(size_t id);
    void remove(size_t id);
    
    // Persistence
    void save_to_file(const char* filename);
    void load_from_file(const char* filename);
    
        // Optional
        size_t size();  // number of entries
    void clear();
};