#ifndef HNSW_H
#define HNSW_H

#include <stdint.h>
#include <stddef.h>
#include <exception>
#include <string>
#include <sstream>

struct AriNode {
    size_t id;
    float* vector;
    u_int8_t h_lvl;
    AriNode*** niblings;
    u_int8_t* nibling_count; 
    double inv_mag;

    std::string toString() const;

};

class HNSW {
public:
    HNSW(int32_t vl, u_int8_t ms = 16, u_int8_t md = 32, u_int16_t ic = 128, u_int16_t sc = 64);
    ~HNSW();

    AriNode* entry;
    size_t size;
    u_int8_t max_sparse;
    u_int8_t max_dense;
    u_int8_t m_lvl;
    u_int8_t search_candidates;
    u_int16_t insert_candidates;
    u_int16_t vlen;

    bool cossmlr_compare(AriNode* comp, AriNode* a, AriNode* b);
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