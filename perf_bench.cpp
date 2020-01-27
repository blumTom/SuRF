#include <iostream>
#include <random>

#include "PerfEvent.hpp"
#include "include/surf.hpp"
#include "tlx/tlx/container.hpp"

#include "bench/bench.hpp"

using namespace surf;


template <typename KeyType>
struct traits_nodebug : tlx::btree_default_traits<KeyType, KeyType> {
    static const bool self_verify = true;
    static const bool debug = false;

    static const int leaf_slots = 1000000;
    static const int inner_slots = 1000000;
};

int main() {
    const std::string path = "../../evaluation/";
    std::ofstream build_file(path + "instructions.csv", std::ofstream::trunc);
    std::ofstream lookup_file(path + "cycles.csv", std::ofstream::trunc);
    std::ofstream llcmisses_file(path + "LLC-misses.csv", std::ofstream::trunc);
    std::ofstream ipc_file(path + "IPC.csv", std::ofstream::trunc);
    build_file << "keys" << ";btree_build" << ";surf_build" << ";btree_lookup" << ";surf_lookup" << ";btree_insert" << ";surf_insert" << ";surf_size" << std::endl;
    lookup_file << "keys" << ";btree_build" << ";surf_build" << ";btree_lookup" << ";surf_lookup" << ";btree_insert" << ";surf_insert" << ";surf_size" << std::endl;
    llcmisses_file << "keys" << ";btree_build" << ";surf_build" << ";btree_lookup" << ";surf_lookup" << ";btree_insert" << ";surf_insert" << ";surf_size" << std::endl;
    ipc_file << "keys" << ";btree_build" << ";surf_build" << ";btree_lookup" << ";surf_lookup" << ";btree_insert" << ";surf_insert" << ";surf_size" << std::endl;
    build_file = std::ofstream(path + "instructions.csv", std::ofstream::app);
    lookup_file = std::ofstream(path + "cycles.csv", std::ofstream::app);
    llcmisses_file = std::ofstream(path + "LLC-misses.csv", std::ofstream::app);
    ipc_file = std::ofstream(path + "IPC.csv", std::ofstream::app);

    //Create uniformly distributed random keys
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dis;

    int generatedKeys = 20000000;
    std::vector<uint32_t> randomKeys;
    for (unsigned int i = 0; i < generatedKeys; i++) {
        uint32_t generatedKey = dis(gen);
        randomKeys.emplace_back(generatedKey);
    }
    std::sort(randomKeys.begin(), randomKeys.end(), std::less<uint64_t>());


    //Load zipfian distributed random keys
    std::string txn_file = "../bench/workloads/txn_";
    txn_file += "randint";
    txn_file += "_";
    txn_file += "zipfian";
    std::vector<std::string> txn_keys;
    bench::loadKeysFromFile(txn_file, true, txn_keys);

    std::cout << "TXN_Keys: " << txn_keys.size() << "\n";


    int step = 100000;
    for (int insertedKeysCount = step; insertedKeysCount<generatedKeys; insertedKeysCount = insertedKeysCount + step) {
        std::cout << "insertedKeysCount: " << insertedKeysCount << "\n";
        build_file << insertedKeysCount;
        lookup_file << insertedKeysCount;
        llcmisses_file << insertedKeysCount;
        ipc_file << insertedKeysCount;
        build_file.flush();
        lookup_file.flush();
        llcmisses_file.flush();
        ipc_file.flush();

        typedef tlx::btree_multimap<uint32_t,
                uint64_t,
                std::less<unsigned int>,
                traits_nodebug<unsigned int> >
                btree_type;
        btree_type bt;
        SuRF<uint32_t,uint64_t>* surf;


        BenchmarkParameters params;
        params.setParam("name","build_perf");
        {
            std::vector<std::pair<uint32_t,uint64_t>> insertKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                insertKeys.emplace_back(std::make_pair(randomKeys[i], static_cast<uint64_t>(i+1)));
            }
            PerfEventBlock e(1, params, false);
            bt.bulk_load(insertKeys.begin(),insertKeys.end());
        }

        params.setParam("name","build_surf");
        {
            std::vector<std::pair<uint32_t,uint64_t >> insertKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                insertKeys.emplace_back(std::make_pair(randomKeys[i], static_cast<uint64_t>(i+1)));
            }
            PerfEventBlock e(1, params, false);

            surf = new SuRF<uint32_t,uint64_t>(insertKeys, true, 3, surf::kNone, 0, 0,uint32ToByteVector);
        }

        params.setParam("name","lookup_perf");
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

        params.setParam("name","lookup_surf");
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

        params.setParam("name","insert_btree");
        int inserts_count = 1;
        {
            std::vector<uint32_t> lookupKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                lookupKeys.emplace_back(dis(gen));
            }
            PerfEventBlock e(inserts_count, params, false);

            for (size_t i = 0; i < inserts_count; i++) {
                bt.insert2(lookupKeys[i],i);
                //bt.find(lookupKeys[i])->second;
            }
        }


        params.setParam("name","insert_surf");
        {
            std::vector<std::pair<uint32_t,uint64_t>> lookupKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                lookupKeys.emplace_back(std::make_pair(dis(gen),i+insertedKeysCount));
            }

            std::function<std::vector<label_t>(const uint64_t)> keyFromValueDerivator = [&lookupKeys](const uint64_t& value) { return uint32ToByteVector(lookupKeys[value].first); };

            PerfEventBlock e(inserts_count, params, false);

            for (size_t i = 0; i < inserts_count; i++) {
                surf->insert(lookupKeys[i].first,lookupKeys[i].second,keyFromValueDerivator);
            }
        }

        /*params.setParam("name","lookup_perf_zipfian");
        {
            std::vector<uint32_t> lookupKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                lookupKeys.emplace_back(stringToUint32(txn_keys[i]));
            }
            PerfEventBlock e(queries_count, params, false);

            for (size_t i = 0; i < queries_count; i++) {
                bt.find(lookupKeys[i])->second;
            }
        }

        params.setParam("name","lookup_surf");
        {
            std::vector<uint32_t> lookupKeys;
            for (int i=0; i<insertedKeysCount; i++) {
                lookupKeys.emplace_back(stringToUint32(txn_keys[i]));
            }
            PerfEventBlock e(queries_count, params, false);

            for (size_t i = 0; i < queries_count; i++) {
                surf->lookupKey(lookupKeys[i]);
            }
        }*/

        surf->destroy();

        build_file << ";" << surf->getMemoryUsage() << std::endl;
        lookup_file << ";" << surf->getMemoryUsage() << std::endl;
        llcmisses_file << ";" << surf->getMemoryUsage() << std::endl;
        ipc_file << ";" << surf->getMemoryUsage() << std::endl;
    }

    build_file.flush();
    build_file.close();
    lookup_file.flush();
    lookup_file.close();
    llcmisses_file.flush();
    llcmisses_file.close();
    ipc_file.flush();
    ipc_file.close();
}
