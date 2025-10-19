#include <fstream>    
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <queue>
#include <functional>
#include <set>

float Q_magv( float* vector, int size){
    float mag = 0.0F;
    for(int i = 0; i < size; i++){
        mag += vector[i] * vector[i]; 
    }
    long i;
    float x2, y;
    float threehalfs = 1.5F;
    x2 = mag * 0.5F;
	y  = mag;
	i  = * ( long * ) &y;                       // evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // add this if weird stuff starts happening!!
	return y;
}

struct AriNode {
    std::size_t hash;
    float* vector;
    uint8_t h_lvl;
    AriNode*** niblings;
    uint8_t* nibling_count; 
    float inv_mag;
};

class HNSW {
    AriNode * entry;
    int size;
    uint8_t max_sparse;
    uint8_t max_dense;
    uint8_t m_lvl;
    uint8_t search_candidates;
    uint16_t insert_candidates;
    uint16_t vlen; 


    HNSW(int vl, uint8_t ms = 16, uint8_t md = 32, uint16_t ic = 128, uint16_t sc = 64){
        size = 0;
        vlen = vl;
        max_sparse = ms;
        max_dense = md;
        insert_candidates = ic;
        search_candidates = sc;
        //dont plan on using larger than uint8_t max_dense so capped at 5 
        m_lvl = (int) std::floor(1/std::logf(max_dense));
        srand(time(NULL));
    }

    short insert(size_t hash, float* vector){
        AriNode* toInsert = new AriNode();
        toInsert->hash = hash;
        toInsert->vector = vector;

        toInsert->inv_mag = Q_magv(vector, vlen); 
        //Level calculation uniform random exponential decay
        uint8_t level = std::floorf(-std::logf((float) rand() / RAND_MAX) * m_lvl);
        toInsert->h_lvl = level;
        toInsert->niblings = new AriNode**[level];
        
        auto compare = [toInsert, this](AriNode* a, AriNode * b){
            float dot_a = 0.0F, dot_b = 0.0F;
            for(int i = 0; i < vlen; i++){
                dot_a += toInsert->vector[i] * a->vector[i];
                dot_b += toInsert->vector[i] * b->vector[i];
            }
            return dot_a * a->inv_mag > dot_b * b->inv_mag;
        };
        std::priority_queue<AriNode*, std::vector<AriNode*>, decltype(compare)> pqueue(compare);
        std::set<AriNode*, decltype(compare)> visited; 
        pqueue.push(entry);
        for(int i = m_lvl; i >= 0; i--){
            int added = 0;
            while(!(pqueue.empty() || pqueue.size() + visited.size() >= insert_candidates)){
                AriNode* cur = pqueue.top();
                visited.insert(cur);
                for(int j = 0; j < cur->nibling_count[i]; j++){
                    if(visited.find(cur->niblings[level][j]) != visited.end()){
                        pqueue.push(cur->niblings[level][j]);
                    }
                }
            }
        }
    }

    short find(){

    }

    short remove(){

    }
};