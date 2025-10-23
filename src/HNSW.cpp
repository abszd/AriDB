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
       << ", h_lvl=" << (int)h_lvl ;
    //    << ", inv_mag=" << inv_mag;
    
    // ss << ", vector=[";
    // // Print first 5 elements or less if vector is shorter
    // // Note: We don't store vector length in AriNode, so this assumes you know it
    // ss << "...";  // Can't print without knowing vlen
    // ss << "]";
    
    ss << ", neighbors={";
    for(int32_t i = 0; i <= h_lvl; i++){
        ss << "lvl" << i << ":[";
        for(int32_t j = 0; j < nibling_count[i]; j++){
            ss << niblings[i][j]->id;
            if(j < nibling_count[i]-1) ss << ",";
        }
        ss << "]: " << (int)nibling_count[i] << " ";
        if(i < h_lvl) ss << ", ";
    }
    ss << "}]";
    
    return ss.str();
}

HNSW::HNSW(int32_t vl, uint8_t ms, uint8_t md, uint16_t ic, uint16_t sc){
    if(vl <= 0 || ms == 0 || md == 0) 
        throw HNSWException(HNSWError::INVALID_PARAMS);
    size = 0;
    entry = nullptr;
    vlen = vl;
    max_sparse = ms;
    max_dense = md;
    insert_candidates = ic;
    search_candidates = sc;
    //dont plan on using larger than uint8_t max_dense so capped at 5 
    m_lvl = 16;
    srand(1);
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
float HNSW::cossmlr_compare(AriNode* comp, AriNode* a, AriNode* b){
    float dot_a = 0.0F, dot_b = 0.0F;
    for(int32_t i = 0; i < vlen; i++){
        dot_a += comp->vector[i] * a->vector[i];
        dot_b += comp->vector[i] * b->vector[i];
    }
    return dot_a * a->inv_mag - dot_b * b->inv_mag;
}

uint8_t HNSW::bs_ins_link(AriNode * target, AriNode* sorted, uint8_t level){
    uint8_t c = sorted->nibling_count[level], l = 0, r = c - 1;
    if(c == 0){
        sorted->nibling_count[l] = 1;
        sorted->niblings[level][l] = target;
        return 0;
    }
    printf("%i", r);
    while(l < r){
        uint8_t m = (l+r)/2;
        if(cossmlr_compare(sorted, target, sorted->niblings[level][m]) >= 0){
            r = m ;
        } 
        else {
            l = m+1;
        }
    }
    for(int32_t j = c; j > l; j--){
        sorted->niblings[level][j] = sorted->niblings[level][j-1];
    }   
    sorted->niblings[level][l] = target;
    if(c < level == 0 ? max_dense : max_sparse )
        sorted->nibling_count[level]++;
    return l;
}
/**
 * binary search a vector against a target
 */
uint8_t HNSW::bs_vec(AriNode * target, AriNode * sorted, uint8_t level){

    uint8_t c = sorted->nibling_count[level], l = 0, r = c - 1;
    if(c == 0){
        return 0;
    }
    printf("%i", r);
    while(l < r){
        uint8_t m = (l+r)/2;
        if(cossmlr_compare(sorted, target, sorted->niblings[level][m]) >= 0){
            r = m ;
        } 
        else {
            l = m+1;
        }
    }
    return l;
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
    uint8_t level = 0;
    float r = (float)rand() / RAND_MAX;
    if(r > 0.0f && r < 1.0f){
        float m_l = 1.0f / std::logf(max_dense);
        level = (uint8_t)std::min((int32_t)m_lvl, (int32_t)std::floor(-std::logf(r) * m_l));
    }
    toInsert->h_lvl = level;
    toInsert->niblings = new AriNode**[level+1]();
    toInsert->nibling_count = new uint8_t[level+1]();
    auto compare = [toInsert, this](AriNode* a, AriNode * b){
        float dot_a = 0.0F, dot_b = 0.0F;
        for(int32_t i = 0; i < vlen; i++){
            dot_a += toInsert->vector[i] * a->vector[i];
            dot_b += toInsert->vector[i] * b->vector[i];
        }
        return dot_a * a->inv_mag > dot_b * b->inv_mag;
    };
    //hypothetically min distance node from level m_lvl+1
    if(entry == nullptr || toInsert->h_lvl > entry->h_lvl){
        entry = toInsert;
        printf("entry created: %s\n", toInsert->toString().c_str());
    }
 
    AriNode * max = entry;
    for(int i = entry->h_lvl; i >= 0; i--){
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
            pqueue.pop();
        }

        //set max node as first in set
        auto iter = visited.begin(); 
        max = *iter;

        //if were at a level where we should add the node 
        if(i <= level){
            int16_t m_niblings = i == 0 ? max_dense : max_sparse;
            toInsert->niblings[i] = new AriNode*[m_niblings];
            //iterate through the ordered set 
            for(;iter != visited.end() && toInsert->nibling_count[i] < m_niblings; iter++){
                AriNode* cur = *iter;
                // insert cur to insert
                bs_ins_link(cur, toInsert, i);
                //if the neighbor has m_niblings nodes as neighbors then we might need to swap it
                uint8_t cur_nibling_count = cur->nibling_count[i];
                if(cur_nibling_count < m_niblings){
                    uint8_t idx = bs_ins_link(toInsert, cur, i);
                    printf("%s\n", toInsert->toString().c_str());
                }
                else if(cossmlr_compare(cur, toInsert, cur->niblings[i][cur_nibling_count - 1])){
                    uint8_t idx = bs_vec(toInsert, cur, i);
                    AriNode * evicted = cur->niblings[i][cur_nibling_count-1];
                    //shift array, remove bidirectional from evicted node and set nibling
                    for(int32_t j = cur_nibling_count-1; j > idx; j--){
                        cur->niblings[i][j] = cur->niblings[i][j-1];
                    }
                    cur->niblings[i][idx] = toInsert;
                    //remove bidirectional linkage for evicted node
                    idx = bs_vec(cur, evicted, i);
//                     if(idx < evicted->nibling_count[i] && evicted->niblings[i][idx] == cur){
//                         for(int32_t j = idx; j < evicted->nibling_count[i]-1; j++){
//                             evicted->niblings[i][j] = evicted->niblings[i][j+1];
//                         }
//                         evicted->nibling_count[i]--;
//                     } else {
//     printf("Expected to find cur (id=%zu) at index %d, but found id=%zu\ncur: %s\nevicted: %s\n", 
//            cur->id, (int)idx, 
//            idx < evicted->nibling_count[i] ? evicted->niblings[i][idx]->id : 999999,
//         cur->toString().c_str(), evicted->toString().c_str());
//     throw HNSWException(HNSWError::NO_EVICTBACKLINK);
// }
                }
            }
        }
    }
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