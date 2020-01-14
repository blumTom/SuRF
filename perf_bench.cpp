#include <iostream>
#include <random>

#include "PerfEvent.hpp"
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
    //Create uniformly distributed random keys
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;

    int generatedKeys = 15000000;
    std::vector<uint32_t> randomKeys;
    for (unsigned int i = 0; i < generatedKeys; i++) {
        uint32_t generatedKey = dis(gen);
        randomKeys.emplace_back(generatedKey);
    }
    std::sort(randomKeys.begin(), randomKeys.end(), std::less<uint64_t>());


    int step = 500000;
    {
        BenchmarkParameters params;
        //params.setParam("name","Headline  ");
        PerfEventBlock e(1, params, true);
    }
    for (int insertedKeysCount = step; insertedKeysCount<generatedKeys; insertedKeysCount = insertedKeysCount + step) {
        std::cout << "insertedKeysCount: " << insertedKeysCount << "\n";

        BenchmarkParameters params;
        //params.setParam("name","Build surf");

        typedef tlx::btree_multimap<uint32_t,
                uint64_t,
                std::less<unsigned int>,
                traits_nodebug<unsigned int> >
                btree_type;
        btree_type bt;
        {
            std::vector<uint32_t> insertKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                insertKeys.emplace_back(randomKeys[i]);
            }
            PerfEventBlock e(1, params, false);
            for (uint64_t i=0; i<randomKeys.size(); i++) {
                bt.insert2(randomKeys[i],i+1);
            }
        }

        //params.setParam("name","Lookup UD");
        int queries_count = 1000;
        {
            std::vector<uint32_t> lookupKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                lookupKeys.emplace_back(dis(gen));
            }
            PerfEventBlock e(queries_count, params, false);

            for (size_t i = 0; i < queries_count; i++) {
                bt.find(lookupKeys[i])->second;
            }
        }
    }
    for (int insertedKeysCount = step; insertedKeysCount<generatedKeys; insertedKeysCount = insertedKeysCount + step) {
        std::cout << "insertedKeysCount: " << insertedKeysCount << "\n";

        BenchmarkParameters params;
        //params.setParam("name","Build surf");
        SuRF *surf;
        {
            std::vector<uint32_t> insertKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                insertKeys.emplace_back(randomKeys[i]);
            }
            PerfEventBlock e(1, params, false);
            surf = new SuRF(insertKeys, true, 3, surf::kNone, 0, 0);
        }

        //params.setParam("name","Lookup UD");
        int queries_count = 1000;
        {
            std::vector<uint32_t> lookupKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                lookupKeys.emplace_back(dis(gen));
            }
            PerfEventBlock e(queries_count, params, false);

            for (size_t i = 0; i < queries_count; i++) {
                surf->lookupKey(lookupKeys[i]);
            }
        }
    }
}
