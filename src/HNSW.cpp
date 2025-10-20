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
    /**
     * Fast magnitude of vector
     */
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
    
    /**
     * returns if a is closer to comp than b
     */
    bool cossmlr_compare(AriNode* comp, AriNode* a, AriNode* b){
        float dot_a = 0.0F, dot_b = 0.0F;
        for(int i = 0; i < vlen; i++){
            dot_a += comp->vector[i] * a->vector[i];
            dot_b += comp->vector[i] * b->vector[i];
        }
        return dot_a * a->inv_mag > dot_b * b->inv_mag;
    }

    
    /**
     * insert a node into HNSW
     */
    short insert(size_t hash, float* vector){
        AriNode* toInsert = new AriNode();
        toInsert->hash = hash;
        toInsert->vector = vector;

        toInsert->inv_mag = Q_magv(vector, vlen); 
        //Level calculation uniform random exponential decay
        uint8_t level = std::floorf(-std::logf((float) rand() / RAND_MAX) * m_lvl);
        toInsert->h_lvl = level;
        toInsert->niblings = new AriNode**[level+1]();
        toInsert->nibling_count = new uint8_t[level+1]();
        auto compare = [toInsert, this](AriNode* a, AriNode * b){
            float dot_a = 0.0F, dot_b = 0.0F;
            for(int i = 0; i < vlen; i++){
                dot_a += toInsert->vector[i] * a->vector[i];
                dot_b += toInsert->vector[i] * b->vector[i];
            }
            return dot_a * a->inv_mag > dot_b * b->inv_mag;
        };
        //hypothetically min distance node from level m_lvl+1
        AriNode * max = entry;
        if(entry == nullptr){
            entry = toInsert;
            return 0;
        }
        for(int i = m_lvl; i >= 0; i--){
            // create pqueue and set, explore nodes and add every explored node to set
            // this way we cna have better interconnectivity and connect distant parts better  
            std::priority_queue<AriNode*, std::vector<AriNode*>, decltype(compare)> pqueue(compare);
            std::set<AriNode*, decltype(compare)> visited(compare); 
            //push closest node from last level
            pqueue.push(max);
            
            //look for next closest nodes until we dont have any or have searched ic amount of nodes
            while(!(pqueue.empty() || visited.size() >= insert_candidates)){
                AriNode* cur = pqueue.top();
                visited.insert(cur);
                for(int j = 0; j < cur->nibling_count[i]; j++){
                    //if not visited add to pqueue
                    if(visited.find(cur->niblings[i][j]) == visited.end()){
                        pqueue.push(cur->niblings[i][j]);
                    } 
                }
                //remove node from pqueue
                pqueue.pop();
            }

            //set max node as first in set
            auto iter = visited.begin(); 
            max = (AriNode*)*iter;

            //if were at a level where we should add the node 
            if(i <= level){
                int16_t m_niblings = i == 0 ? max_dense : max_sparse;
                toInsert->niblings[i] = new AriNode*[m_niblings];
                uint8_t nibs = 0;
                //iterate through the ordered set 
                for(;iter != visited.end() && nibs < m_niblings; iter++){
                    AriNode* cur = *iter;
                    toInsert->niblings[i][nibs] = cur;
                    //if the neighbor has m_niblings nodes as neighbors then we might need to swap it
                    uint8_t cur_nibling_count = cur->nibling_count[i];
                    if(cur_nibling_count < m_niblings){
                        cur->niblings[i][cur->nibling_count[i]++] = toInsert;
                    }
                    else if(cossmlr_compare(cur, toInsert, cur->niblings[i][cur_nibling_count - 1])){
                        
                        uint8_t l = 0, r = cur_nibling_count -1;
                        //find first nibling that is farther than our insert
                        
                        while(l < r){
                            uint8_t m = (l + r) / 2;
                            if(cossmlr_compare(cur, toInsert, cur->niblings[i][m])){
                                r = m;
                            } else {
                                l = m + 1;
                            }
                        }
                        AriNode * evicted = cur->niblings[i][cur_nibling_count-1];
                        //shift array, remove bidirectional from evicted node and set nibling
                        for(int j = cur_nibling_count-1; j > l; j--){
                            cur->niblings[i][j] = cur->niblings[i][j-1];
                        }
                        cur->niblings[i][l] = toInsert;

                        //remove bidirectional linkage for evicted node
                        l = 0, r = evicted->nibling_count[i] - 1;
                        while(l < r){
                            uint8_t m = (l+r)/2;
                            if(cossmlr_compare(evicted, cur, evicted->niblings[i][m])){
                                r = m;
                            } else {
                                l = m+1;
                            }
                        }
                        //if we found the linkage in the other node (surely so) then shift it into non existence 
                        if(evicted->niblings[i][l] == cur){
                            for(int j = l; j < evicted->nibling_count[i]-1; j++){
                                evicted->niblings[i][j] = evicted->niblings[i][j+1];
                            }
                            evicted->nibling_count[i]--;
                        }
                    }
                    nibs++;
                }
            }
        }
        return 0;
    }

    short find(){
        
    }

    short remove(){

    }
};