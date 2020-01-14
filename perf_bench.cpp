#include <iostream>
#include <random>

#include "PerfEvent.hpp"
#include "include/surf.hpp"

using namespace surf;

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

    for (int insertedKeysCount = step; insertedKeysCount<generatedKeys; insertedKeysCount = insertedKeysCount + step) {
        std::cout << "insertedKeysCount: " << insertedKeysCount << "\n";

        BenchmarkParameters params;
        params.setParam("name","Build surf");
        SuRF *surf;
        {
            std::vector<uint32_t> insertKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                insertKeys.emplace_back(randomKeys[i]);
            }
            PerfEventBlock e(1, params, true);
            surf = new SuRF(insertKeys, true, 3, surf::kNone, 0, 0);
        }

        params.setParam("name","Lookup UD");
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