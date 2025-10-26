#include <iostream> 
#include <vector>
#include <cstdint>
#include <stdfloat>
#include <set>

struct Node {
    size_t id;
    std::vector<float> vector;
    uint8_t highest_level;
    double inv_magnitude;
    std::vector<std::vector<Node*>> niblings;  // per level, kept sorted
    
    Node(size_t id, std::vector<float> vec, uint8_t level, double inv_mag) 
        : id(id), vector(vec), highest_level(level), inv_magnitude(inv_mag) {
        niblings.resize(level + 1);
    }
    std::string toString() const;
};

class HNAW {
public:
    HNAW(int32_t vl, uint8_t ms = 16, uint8_t md = 32, uint16_t ic = 128, uint16_t sc = 64);
    ~HNAW();

    int insert_vector(fl)
private:
    Node* entry;
    size_t size;
    uint8_t max_sparse;
    uint8_t max_dense;
    uint8_t m_lvl;
    uint8_t search_candidates;
    uint16_t insert_candidates;
    uint16_t vlen;

    
};

