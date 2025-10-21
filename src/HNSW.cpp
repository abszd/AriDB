#include "HNSW.h"
#include <fstream>    
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <cmath>
#include <ctime>
#include <queue>
#include <functional>
#include <set>
#include <unordered_set>

std::string AriNode::toString() const {
    std::stringstream ss;
    ss << "AriNode[id=" << id 
       << ", h_lvl=" << (int)h_lvl 
       << ", inv_mag=" << inv_mag;
    
    ss << ", vector=[";
    // Print first 5 elements or less if vector is shorter
    // Note: We don't store vector length in AriNode, so this assumes you know it
    ss << "...";  // Can't print without knowing vlen
    ss << "]";
    
    ss << ", neighbors={";
    for(int32_t i = 0; i <= h_lvl; i++){
        ss << "lvl" << i << ":[";
        for(int32_t j = 0; j < nibling_count[i]; j++){
            ss << niblings[i][j]->id;
            if(j < nibling_count[i]-1) ss << ",";
        }
        ss << "]";
        if(i < h_lvl) ss << ", ";
    }
    ss << "}]";
    
    return ss.str();
}

HNSW::HNSW(int32_t vl, u_int8_t ms, u_int8_t md, u_int16_t ic, u_int16_t sc){
    if(vl <= 0 || ms == 0 || md == 0) 
        throw HNSWException(HNSWError::INVALID_PARAMS);
    size = 0;
    entry = nullptr;
    vlen = vl;
    max_sparse = ms;
    max_dense = md;
    insert_candidates = ic;
    search_candidates = sc;
    //dont plan on using larger than u_int8_t max_dense so capped at 5 
    m_lvl = (int32_t) std::floor(std::logf(size));
    srand(time(NULL));
}

HNSW::~HNSW(){
    if(entry == nullptr) return;
    
    std::unordered_set<AriNode*> visited;
    std::queue<AriNode*> to_process;
    to_process.push(entry);
    
    while(!to_process.empty()){
        AriNode* cur = to_process.front();
        to_process.pop();
        
        if(visited.find(cur) != visited.end()) continue;
        visited.insert(cur);
        
        for(int32_t i = 0; i <= cur->h_lvl; i++){
            for(int32_t j = 0; j < cur->nibling_count[i]; j++){
                if(visited.find(cur->niblings[i][j]) == visited.end()){
                    to_process.push(cur->niblings[i][j]);
                }
            }
        }
    }
    
    for(AriNode* node : visited){
        for(int32_t i = 0; i <= node->h_lvl; i++){
            delete[] node->niblings[i];
        }
        delete[] node->niblings;
        delete[] node->nibling_count;
        delete node;
    }
}

/**
 * returns if a is closer to comp than b
 */
bool HNSW::cossmlr_compare(AriNode* comp, AriNode* a, AriNode* b){
    float dot_a = 0.0F, dot_b = 0.0F;
    for(int32_t i = 0; i < vlen; i++){
        dot_a += comp->vector[i] * a->vector[i];
        dot_b += comp->vector[i] * b->vector[i];
    }
    return dot_a * comp->inv_mag * a->inv_mag > dot_b * comp->inv_mag * b->inv_mag;
}

/**
 * insert a node into HNSW
 */
int16_t HNSW::insert(size_t id, float* vector){
    if(vector == nullptr) 
        throw HNSWException(HNSWError::INVALID_VECTOR);
    AriNode* toInsert = new AriNode();
    toInsert->id = id;
    toInsert->vector = vector;
    toInsert->inv_mag = Q_magv(vector, vlen); 
    //Level calculation uniform random exponential decay
    u_int8_t level = 0;
    float r = (float) rand() / RAND_MAX;
    if(r > 0.0F && r < 1.0F){
        level = (u_int8_t) ((float)m_lvl, std::floor(-std::logf(r) * m_l));
    }
    if(entry == nullptr){
        level = m_lvl;
    }
    toInsert->h_lvl = level;
    toInsert->niblings = new AriNode**[level+1]();
    toInsert->nibling_count = new u_int8_t[level+1]();
    auto compare = [toInsert, this](AriNode* a, AriNode * b){
        float dot_a = 0.0F, dot_b = 0.0F;
        for(int32_t i = 0; i < vlen; i++){
            dot_a += toInsert->vector[i] * a->vector[i];
            dot_b += toInsert->vector[i] * b->vector[i];
        }
        return dot_a * toInsert->inv_mag * a->inv_mag > dot_b * toInsert->inv_mag * b->inv_mag;
    };
    //hypothetically min distance node from level m_lvl+1
    AriNode * max;
    if(entry == nullptr){
        entry = toInsert;
        printf("entry created: %s", toInsert->toString().c_str());
    } 
    max = entry;
    

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

            if(visited.find(cur) != visited.end()){
                pqueue.pop();
                continue;
            }

            visited.insert(cur);
            for(int j = 0; j < cur->nibling_count[i]; j++){
                //if not visited add to pqueue
                if(visited.find(cur->niblings[i][j]) == visited.end()){
                    pqueue.push(cur->niblings[i][j]);
                } else {
                    continue;
                }
            }

        }

        //set max node as first in set
        auto iter = visited.begin(); 
        max = *iter;

        //if were at a level where we should add the node 
        if(i <= level){
            int16_t m_niblings = i == 0 ? max_dense : max_sparse;
            toInsert->niblings[i] = new AriNode*[m_niblings];
            u_int8_t nibs = 0;
            //iterate through the ordered set 
            for(;iter != visited.end() && nibs < m_niblings; iter++){
                AriNode* cur = *iter;
                toInsert->niblings[i][nibs] = cur;
                //if the neighbor has m_niblings nodes as neighbors then we might need to swap it
                u_int8_t cur_nibling_count = cur->nibling_count[i];
                if(cur_nibling_count < m_niblings){
                    cur->niblings[i][cur->nibling_count[i]++] = toInsert;
                }
                else if(cossmlr_compare(cur, toInsert, cur->niblings[i][cur_nibling_count - 1])){
                    
                    u_int8_t l = 0, r = cur_nibling_count -1;
                    //find first nibling that is farther than our insert
                    
                    while(l < r){
                        u_int8_t m = (l + r) / 2;
                        if(cossmlr_compare(cur, toInsert, cur->niblings[i][m])){
                            r = m;
                        } else {
                            l = m + 1;
                        }
                    }
                    AriNode * evicted = cur->niblings[i][cur_nibling_count-1];
                    //shift array, remove bidirectional from evicted node and set nibling
                    for(int32_t j = cur_nibling_count-1; j > l; j--){
                        cur->niblings[i][j] = cur->niblings[i][j-1];
                    }
                    cur->niblings[i][l] = toInsert;

                    //remove bidirectional linkage for evicted node
                    l = 0, r = evicted->nibling_count[i] - 1;
                    while(l < r){
                        u_int8_t m = (l+r)/2;
                        if(cossmlr_compare(evicted, cur, evicted->niblings[i][m])){
                            r = m;
                        } else {
                            l = m+1;
                        }
                    }
                    //if we found the linkage in the other node (surely so) then shift it into non existence 
                    if(evicted->niblings[i][l] == cur){
                        for(int32_t j = l; j < evicted->nibling_count[i]-1; j++){
                            evicted->niblings[i][j] = evicted->niblings[i][j+1];
                        }
                        evicted->nibling_count[i]--;
                    } else {
                        printf("toInsert: %s\ncurrent: %s\nevicted: %s\n", toInsert->toString().c_str(), cur->toString().c_str(), evicted->toString().c_str());
                        throw HNSWException(HNSWError::NO_EVICTBACKLINK);
                    }
                }
                nibs++;
            }
            toInsert->nibling_count[i] = nibs;
        }
    }
    printf("%s\n", toInsert->toString().c_str());
    size++;
    return 0;
}

int16_t HNSW::scrub(AriNode* node){
    return 0;
}

int16_t HNSW::find(){
    if(entry == nullptr) 
        throw HNSWException(HNSWError::EMPTY_DB);
    return 0;
}

int16_t HNSW::remove(){
    return 0;
}

int16_t HNSW::clear(){
    return 0;
}

const char* HNSWException::what() const noexcept {
    switch(code) {
        case HNSWError::NOT_FOUND: return "Node not found";
        case HNSWError::INVALID_VECTOR: return "Invalid vector";
        case HNSWError::EMPTY_DB: return "Database is empty";
        case HNSWError::INVALID_PARAMS: return "Invalid parameters";
        case HNSWError::NO_EVICTBACKLINK: return "Evicted Node is not Linked to Widow Nibling";
        default: return "Unknown error";
    }
}

/**
 * Fast inverse magnitude of vector
 */
float Q_magv(float* vector, int32_t size){
    float mag = 0.0F;
    for(int32_t i = 0; i < size; i++){
        mag += vector[i] * vector[i]; 
    }
    // long i;
    // float x2, y;
    // float threehalfs = 1.5F;
    // x2 = mag * 0.5F;
    // y  = mag;
    // i  = * ( long * ) &y;                       // evil floating point bit level hacking
    // i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
    // y  = * ( float * ) &i;
    // y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
    // y  = y * ( threehalfs - ( x2 * y * y ) );   // add this if weird stuff starts happening!!
    return 1 / sqrt(mag);
}