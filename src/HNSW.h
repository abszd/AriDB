#ifndef HNSW_H
#define HNSW_H

#include <stdint.h>
#include <stddef.h>
#include <exception>
#include <string>
#include <sstream>

struct AriNode {
    size_t id;
    uint8_t h_lvl;
    float* vector;
    AriNode*** niblings;
    uint8_t* nibling_count; 
    double inv_mag;
    long address;
    std::string toString() const;
};

class HNSW {
public:
    HNSW(int32_t vl, uint8_t ms = 16, uint8_t md = 32, uint16_t ic = 128, uint16_t sc = 64);
    ~HNSW();

    AriNode* entry;
    size_t size;
    uint8_t max_sparse;
    uint8_t max_dense;
    uint8_t m_lvl;
    uint8_t search_candidates;
    uint16_t insert_candidates;
    uint16_t vlen;

    float cossmlr_compare(AriNode* comp, AriNode* a, AriNode* b);
    uint8_t bs_vec(AriNode* target, AriNode* sorted, uint8_t lvl);
    uint8_t bs_ins_link(AriNode* target, AriNode* sorted, uint8_t lvl);
    uint8_t bs_rem_link(AriNode* target, AriNode* sorted, uint8_t lvl);
    int16_t insert(size_t id, float* vector);
    int16_t scrub(AriNode* node);
    int16_t find();
    int16_t remove();
    int16_t clear();
};

enum class HNSWError {
    NOT_FOUND,
    INVALID_VECTOR,
    EMPTY_DB,
    INVALID_PARAMS,
    NO_EVICTBACKLINK
};

class HNSWException : public std::exception {
    HNSWError code;
public:
    HNSWException(HNSWError e) : code(e) {}
    HNSWError error() const { return code; }
    const char* what() const noexcept override;
};

float Q_magv(float* vector, int32_t size);

#endif