#ifndef TESTSUITE_H_
#define TESTSUITE_H_

#include <fstream>

#include "surf.hpp"

using namespace surf;

    static const std::string kFilePath = "../../test/words.txt";
    static const int kTestSize = 1000;
    static const int kIntTestSize = 1001;
    static const uint64_t kIntTestSkip = 10;
    extern std::vector<std::pair<std::vector<label_t>,uint64_t>> words;
    extern std::vector<std::pair<std::vector<label_t>,uint64_t>> words_dup;
    extern std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_;
    extern std::vector<std::pair<std::vector<label_t>,uint64_t>> words_trunc_;
    extern std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_trunc_;

    static int getCommonPrefixLen(const std::vector<label_t> &a, const std::vector<label_t> &b) {
        int len = 0;
        while ((len < (int) a.size()) && (len < (int) b.size()) && (a[len] == b[len]))
            len++;
        return len;
    }

    static int getMax(int a, int b) {
        if (a < b)
            return b;
        return a;
    }

    static void truncateSuffixes(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys, std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys_trunc) {
        assert(keys.size() > 1);

        int commonPrefixLen = 0;
        for (unsigned i = 0; i < keys.size(); i++) {
            if (i == 0) {
                commonPrefixLen = getCommonPrefixLen(keys[i].first, keys[i + 1].first);
            } else if (i == keys.size() - 1) {
                commonPrefixLen = getCommonPrefixLen(keys[i - 1].first, keys[i].first);
            } else {
                commonPrefixLen = getMax(getCommonPrefixLen(keys[i - 1].first, keys[i].first),getCommonPrefixLen(keys[i].first, keys[i + 1].first));
            }

            if (commonPrefixLen < (int) keys[i].first.size()) {
                std::vector<label_t> subVector;
                for (int j=0; j<commonPrefixLen + 1; j++) {
                    subVector.emplace_back(keys[i].first[j]);
                }
                keys_trunc.push_back({subVector,keys[i].second});
            } else {
                keys_trunc.push_back(keys[i]);
                keys_trunc[i].first.emplace_back(kTerminator);
            }
        }
    }

    static void fillinInts() {
        for (uint64_t i = 0; i < kIntTestSize; i += kIntTestSkip) {
            ints_.push_back({uint64ToByteVector(i),i});
        }
    }

    static void loadWordList() {
        std::ifstream infile(kFilePath);
        std::string keyStr;
        int count = 0;
        while (infile.good() && count < kTestSize) {
            infile >> keyStr;
            std::vector<label_t> key;
            for (int i=0; i<keyStr.length(); i++) {
                key.emplace_back(keyStr[i]);
            }
            words.push_back({key,count});
            words_dup.push_back({key,2 * count});
            words_dup.push_back({key,2 * count + 1});
            count++;
        }
        truncateSuffixes(words,words_trunc_);
        fillinInts();
        truncateSuffixes(ints_,ints_trunc_);
    }

#endif // TESTSUITE_H_