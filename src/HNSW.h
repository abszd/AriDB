struct AriNode {
    std::size_t id;
    float* vector;
    uint8_t h_lvl;
    AriNode*** niblings;
    uint8_t* nibling_count; 
    float inv_mag;
};

class HNSW{
public:
    HNSW(int vl, uint8_t ms = 16, uint8_t md = 32, uint16_t ic = 128, uint16_t sc = 64);

    AriNode* entry;
    int size;
    uint8_t max_sparse;
    uint8_t max_dense;
    uint8_t m_lvl;
    uint8_t search_candidates;
    uint16_t insert_candidates;
    uint16_t vlen; 

    bool cossmlr_compare(AriNode* comp, AriNode* a, AriNode* b);
    short insert(size_t id, float* vector);
    short scrub(AriNode* node);

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
    
    const char* what() const noexcept override {
        switch(code) {
            case HNSWError::NOT_FOUND: return "Node not found";
            case HNSWError::INVALID_VECTOR: return "Invalid vector";
            case HNSWError::EMPTY_DB: return "Database is empty";
            case HNSWError::INVALID_PARAMS: return "Invalid parameters";
            case HNSWError::NO_EVICTBACKLINK: return "Evicted Node is not Linked to Widow Nibling";
            default: return "Unknown error";
        }
    }
};

/**
 * Fast inverse magnitude of vector
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
    y  = y * ( threehalfs - ( x2 * y * y ) );   // add this if weird stuff starts happening!!
    return y;
}
