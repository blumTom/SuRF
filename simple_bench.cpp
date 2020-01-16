#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <functional>
#include <chrono>

#include "include/surf.hpp"
#include "tlx/tlx/container.hpp"

using namespace surf;

template <typename KeyType>
struct traits_nodebug : tlx::btree_default_traits<KeyType, KeyType> {
    static const bool self_verify = true;
    static const bool debug = false;

    static const int leaf_slots = 1000000;
    static const int inner_slots = 1000000;
};

int main() {
    std::vector<std::string> keys = {
            "f",
            "far",
            "fast",
            "s",
            "top",
            "toy",
            "trie",
    };

    typedef tlx::btree_multimap<
            uint32_t,
            uint64_t,
            std::less<unsigned int>,
            traits_nodebug<unsigned int> >
            btree_type;
    btree_type bt;

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;
    std::vector<uint32_t> randomKeys;
    for (unsigned int i = 0; i < 1000000; i++) {
        uint32_t generatedKey = dis(gen);
        randomKeys.emplace_back(generatedKey);
    }
    std::sort(randomKeys.begin(), randomKeys.end(), std::less<uint64_t>());
    std::vector<std::pair<uint32_t,uint64_t>> randomKeyValues;
    for (unsigned int i = 0; i < 1000000; i++) {
        randomKeyValues.emplace_back(std::make_pair(randomKeys[i], i));
    }


    auto start = std::chrono::system_clock::now();
    bt.bulk_load(randomKeyValues.begin(),randomKeyValues.end());
    auto end = std::chrono::system_clock::now();
    std::cout << "BTreeSize: " << bt.size() << "\n";
    std::cout << "BTree Build Time:\t" << (end - start).count() << "[microseconds]\n";


    start = std::chrono::system_clock::now();
    SuRF<uint32_t,uint64_t> *surf = new SuRF<uint32_t,uint64_t>(randomKeyValues, true, 3, surf::kNone, 0, 0, uint32ToByteVector);
    end = std::chrono::system_clock::now();
    std::cout << "SURF Build Time:\t" << (end - start).count() << "[microseconds]\n";


    std::optional<uint64_t> result = std::nullopt;
    uint32_t key = 0;
    int falsePositives = 0;
    int truePositives = 0;
    for (int i=0; i<10; ) {
        key = dis(gen);
        result = surf->lookupKey(key);
        if (result.has_value() && result.value() != bt.find(key)->second) {
            falsePositives++;
        } else if (result.has_value()) {
            truePositives++;
            i++;
        }
    }

    if (falsePositives > truePositives) {
        std::cout << "ErrorRate (TP:FP) => 1:" << static_cast<int>(static_cast<double>(falsePositives) / static_cast<double>(truePositives)) << "\n";
    } else {
        std::cout << "ErrorRate (TP:FP) => 1:" << static_cast<int>(static_cast<double>(truePositives) / static_cast<double>(falsePositives)) << "\n";
    }
    start = std::chrono::system_clock::now();
    std::cout << "BTree Value:\t" << bt.find(key)->second << "\n";
    end = std::chrono::system_clock::now();
    std::cout << "BTree Lookup Time:\t" << (end - start).count() << "[microseconds]\n";
    start = std::chrono::system_clock::now();
    std::cout << "SURF Value:\t" << surf->lookupKey(key).value() << "\n";
    end = std::chrono::system_clock::now();
    std::cout << "SURF Lookup Time:\t" << (end - start).count() << "[microseconds]\n";

    /*std::cout << ((uint64_t) randomKeys[result.value() - 1]) << "\n";
    std::cout << counter << "\n";*/

    return 0;
}
