#include "HNSW.h"
#include <cassert>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>

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
    
    void testFirstInsert() {
        HNSW index(128);
        float* vec = generateRandomVector(128);
        assert(index.insert(1, vec) == 0);
        assert(index.size == 1);  // WILL FAIL with your bug
        assert(index.entry != nullptr);
        assert(index.entry->id == 1);

        delete[] vec;
    }
    
    void testLevelDistribution() {
        HNSW index(128);
        // Insert many nodes and check level distribution
        for(int i = 0; i < 10000; i++) {

            float* vec = generateRandomVector(128);

            index.insert(i, vec);
            // You'd need to expose node levels for this test
        }
        
        // Should follow exponential decay
        // With proper m_lvl, expect ~5000 at level 0, ~2500 at level 1, etc
    }
    
    void testBidirectionalLinks() {
        HNSW index(4, 2, 4);  // Small for testing
        
        std::vector<float*> vecs;
        for(int i = 0; i < 100; i++) {
            float* vec = generateRandomVector(4);
            vecs.push_back(vec);
            index.insert(i, vec);
        }
        
        // Verify every edge is bidirectional
        // Need to traverse and check that if A->B exists, then B->A exists
        // This would catch your NO_EVICTBACKLINK error
    }
    
    void stressTestInsert() {
        HNSW index(128);
        auto start = std::chrono::high_resolution_clock::now();
        for(int i = 0; i < 100000; i++) {
            printf("%i: ", i);
            float* vec = generateRandomVector(128);
            try {
                index.insert(i, vec);
            } catch(HNSWException& e) {
                std::cerr << "Failed at " << i << ": " << e.what() << std::endl;
                break;
            }
            
            if(i>0 && i % 10000 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                std::cout << "Inserted " << i << " in " << ms << "ms" << std::endl;
            }
        }
    }
    
    void testSearchCorrectness() {
        // After implementing search
        // Insert known vectors, search for nearest neighbors
        // Verify correct results
    }
};

int main() {
    HNSWTester tester;
    
    std::cout << "Testing first insert..." << std::endl;
    tester.testFirstInsert();
    
    std::cout << "Testing stress insert..." << std::endl;
    tester.stressTestInsert();
    
    // Add memory leak detection with valgrind:
    // valgrind --leak-check=full ./test
    
    return 0;
}