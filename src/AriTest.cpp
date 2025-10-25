#include "HNSW.h"
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <queue>
#include <unordered_set>

class HNSWTester {
public:
    std::mt19937 gen{1};
    std::uniform_real_distribution<> dis{-1.0, 1.0};

    float* generateRandomVector(int dim) {
        float* vec = new float[dim];
        for(int i = 0; i < dim; i++) {
            vec[i] = dis(gen);
        }
        return vec;
    }
    
    void testSearchAfterInsert() {
        HNSW index(128);
        std::vector<float*> vecs;
        
        // Insert 1000 vectors
        for(int i = 0; i < 1000; i++) {
            float* vec = generateRandomVector(128);
            vecs.push_back(vec);
            index.insert(i, vec);
        }
        
        // Search for each vector we inserted - should find itself as nearest
        // You need to implement search first, but this will verify correctness
        for(int i = 0; i < 1000; i++) {
            // auto results = index.search(vecs[i], 10);
            // assert(results[0].id == i); // Should find itself
        }
    }

    void testBidirectionalLinkIntegrity() {
        HNSW index(4, 2, 4);
        
        for(int i = 0; i < 500; i++) {
            float* vec = generateRandomVector(4);
            index.insert(i, vec);
        }
        
        // Traverse all nodes and verify bidirectional links
        std::unordered_set<AriNode*> visited;
        std::queue<AriNode*> to_check;
        to_check.push(index.entry);
        
        int link_errors = 0;
        while(!to_check.empty()) {
            AriNode* cur = to_check.front();
            to_check.pop();
            
            if(visited.find(cur) != visited.end()) continue;
            visited.insert(cur);
            
            for(int32_t lvl = 0; lvl <= cur->h_lvl; lvl++) {
                for(int32_t j = 0; j < cur->nibling_count[lvl]; j++) {
                    AriNode* neighbor = cur->niblings[lvl][j];
                    
                    // Check if neighbor has cur as a neighbor
                    bool found_backlink = false;
                    for(int32_t k = 0; k < neighbor->nibling_count[lvl]; k++) {
                        if(neighbor->niblings[lvl][k] == cur) {
                            found_backlink = true;
                            break;
                        }
                    }
                    
                    if(!found_backlink) {
                        // std::cout << "BROKEN LINK: Node " << cur->id 
                        //         << " -> Node " << neighbor->id 
                        //         << " at level " << (int)lvl 
                        //         << " but no backlink!" << std::endl;
                        link_errors++;
                    }
                    
                    if(visited.find(neighbor) == visited.end()) {
                        to_check.push(neighbor);
                    }
                }
            }
        }
        
        std::cout << "Checked " << visited.size() << " nodes, found " 
                << link_errors << " broken links" << std::endl;
        assert(link_errors == 0);
    }

    void testSortedNeighborOrdering() {
        HNSW index(4, 2, 4);
        
        for(int i = 0; i < 200; i++) {
            float* vec = generateRandomVector(4);
            index.insert(i, vec);
        }
        
        // Verify that all neighbor arrays are sorted by distance
        std::unordered_set<AriNode*> visited;
        std::queue<AriNode*> to_check;
        to_check.push(index.entry);
        
        while(!to_check.empty()) {
            AriNode* cur = to_check.front();
            to_check.pop();
            
            if(visited.find(cur) != visited.end()) continue;
            visited.insert(cur);
            
            for(int32_t lvl = 0; lvl <= cur->h_lvl; lvl++) {
                for(int32_t j = 0; j < cur->nibling_count[lvl] - 1; j++) {
                    AriNode* a = cur->niblings[lvl][j];
                    AriNode* b = cur->niblings[lvl][j+1];
                    
                    // Verify a is closer than b
                    float cmp = index.cossmlr_compare(cur, a, b);
                    if(cmp < 0) { // b is closer than a - WRONG ORDER
                        std::cout << "UNSORTED: Node " << cur->id 
                                << " level " << (int)lvl 
                                << " neighbor " << j << " (id=" << a->id 
                                << ") is farther than neighbor " << j+1 
                                << " (id=" << b->id << ")" << std::endl;
                        assert(false);
                    }
                    
                    if(visited.find(a) == visited.end()) to_check.push(a);
                }
            }
        }
        
        std::cout << "All neighbor lists properly sorted" << std::endl;
    }

    void testMaxNeighborConstraints() {
        HNSW index(4, 2, 4);
        
        for(int i = 0; i < 300; i++) {
            float* vec = generateRandomVector(4);
            index.insert(i, vec);
        }
        
        std::unordered_set<AriNode*> visited;
        std::queue<AriNode*> to_check;
        to_check.push(index.entry);
        
        while(!to_check.empty()) {
            AriNode* cur = to_check.front();
            to_check.pop();
            
            if(visited.find(cur) != visited.end()) continue;
            visited.insert(cur);
            
            for(int32_t lvl = 0; lvl <= cur->h_lvl; lvl++) {
                uint8_t max_allowed = (lvl == 0) ? index.max_dense : index.max_sparse;
                if(cur->nibling_count[lvl] > max_allowed) {
                    std::cout << "OVERFLOW: Node " << cur->id 
                            << " level " << (int)lvl 
                            << " has " << (int)cur->nibling_count[lvl] 
                            << " neighbors (max=" << (int)max_allowed << ")" << std::endl;
                    assert(false);
                }
                
                for(int32_t j = 0; j < cur->nibling_count[lvl]; j++) {
                    if(visited.find(cur->niblings[lvl][j]) == visited.end()) {
                        to_check.push(cur->niblings[lvl][j]);
                    }
                }
            }
        }
        
        std::cout << "All neighbor counts within limits" << std::endl;
    }

    void testNoSelfLinks() {
        HNSW index(4, 2, 4);
        
        for(int i = 0; i < 200; i++) {
            float* vec = generateRandomVector(4);
            index.insert(i, vec);
        }
        
        std::unordered_set<AriNode*> visited;
        std::queue<AriNode*> to_check;
        to_check.push(index.entry);
        
        while(!to_check.empty()) {
            AriNode* cur = to_check.front();
            to_check.pop();
            
            if(visited.find(cur) != visited.end()) continue;
            visited.insert(cur);
            
            for(int32_t lvl = 0; lvl <= cur->h_lvl; lvl++) {
                for(int32_t j = 0; j < cur->nibling_count[lvl]; j++) {
                    if(cur->niblings[lvl][j] == cur) {
                        std::cout << "SELF-LINK: Node " << cur->id 
                                << " links to itself at level " << (int)lvl << std::endl;
                        assert(false);
                    }
                    
                    if(visited.find(cur->niblings[lvl][j]) == visited.end()) {
                        to_check.push(cur->niblings[lvl][j]);
                    }
                }
            }
        }
        
        std::cout << "No self-links found" << std::endl;
    }
};

    
int main() {
    HNSWTester tester;
    
    
    std::cout << "Testing bidirectional links..." << std::endl;
    tester.testBidirectionalLinkIntegrity();
    
    std::cout << "Testing sorted neighbor ordering..." << std::endl;
    tester.testSortedNeighborOrdering();
    
    std::cout << "Testing max neighbor constraints..." << std::endl;
    tester.testMaxNeighborConstraints();
    
    std::cout << "Testing no self-links..." << std::endl;
    tester.testNoSelfLinks();
    
    
    return 0;
}