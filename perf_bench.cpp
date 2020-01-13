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

    std::vector<uint32_t> randomKeys;
    for (unsigned int i = 0; i < 15000; i++) {
        uint32_t generatedKey = dis(gen);
        randomKeys.emplace_back(generatedKey);
    }
    std::sort(randomKeys.begin(), randomKeys.end(), std::less<uint64_t>());

    SuRF *surf = new SuRF(randomKeys, true, 3, surf::kNone, 0, 0);

    int queries_count = 10;

    BenchmarkParameters params;
    params.setParam("name","SURF Benchmark");

    // benchmark ACT
    {
        PerfEventBlock e(queries_count, params, true);

        uint32_t key = dis(gen);
        for (size_t cntr = 0; cntr < queries_count; cntr++) {
            key = dis(gen);
            surf->lookupKey(key);
        }
    }
}