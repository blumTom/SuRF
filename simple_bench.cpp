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
    for (unsigned int i = 0; i < 15000; i++) {
        uint32_t generatedKey = dis(gen);
        randomKeys.emplace_back(generatedKey);
    }
    std::sort(randomKeys.begin(), randomKeys.end(), std::less<uint64_t>());

    auto start = std::chrono::system_clock::now();
    for (uint64_t i=0; i<randomKeys.size(); i++) {
        bt.insert2(randomKeys[i],i+1);
    }
    auto end = std::chrono::system_clock::now();
    std::cout << "BTreeSize: " << bt.size() << "\n";
    std::cout << "BTree Build Time: " << (end - start).count() << "[ns]\n";

    start = std::chrono::system_clock::now();
    SuRF *surf = new SuRF(randomKeys, true, 3, surf::kNone, 0, 0);
    end = std::chrono::system_clock::now();
    std::cout << "SURF Build Time: " << (end - start).count() << "[ns]\n";

    std::optional<uint64_t> result = std::nullopt;
    uint32_t key = dis(gen);

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
    std::cout << "BTree Value: " << bt.find(key)->second << "\n";
    end = std::chrono::system_clock::now();
    std::cout << "BTree Lookup Time: " << (end - start).count() << "[ns]\n";

    start = std::chrono::system_clock::now();
    std::cout << "SURF Value: " << surf->lookupKey(key).value() << "\n";
    end = std::chrono::system_clock::now();
    std::cout << "SURF Lookup Time: " << (end - start).count() << "[ns]\n";

    /*std::cout << ((uint64_t) randomKeys[result.value() - 1]) << "\n";
    std::cout << counter << "\n";*/

    return 0;
}
