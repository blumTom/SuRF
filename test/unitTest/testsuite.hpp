#ifndef TESTSUITE_H_
#define TESTSUITE_H_

#include <fstream>

#include "surf.hpp"

using namespace surf;

static const std::string kFilePath = "words.txt";
static const int kTestSize = 1000;
static const int kIntTestSize = 1001;
static const uint64_t kIntTestSkip = 10;
extern std::vector<std::pair<std::string, uint64_t>> words;
extern std::vector<std::pair<std::string, uint64_t>> words_dup;
extern std::vector<std::pair<std::string, uint64_t>> ints_;
extern std::vector<std::pair<std::string, uint64_t>> words_trunc_;
extern std::vector<std::pair<std::string, uint64_t>> ints_trunc_;

static int getCommonPrefixLen(const std::string &a, const std::string &b) {
    int len = 0;
    while ((len < (int) a.length()) && (len < (int) b.length()) && (a[len] == b[len]))
        len++;
    return len;
}

static int getMax(int a, int b) {
    if (a < b)
        return b;
    return a;
}

    static void truncateSuffixes(const std::vector<std::pair<std::string,uint64_t>> &keys,
            std::vector<std::pair<std::string,uint64_t>> &keys_trunc) {
        assert(keys.size() > 1);

        int commonPrefixLen = 0;
        for (unsigned i = 0; i < keys.size(); i++) {
            if (i == 0) {
                commonPrefixLen = getCommonPrefixLen(keys[i].first, keys[i+1].first);
            } else if (i == keys.size() - 1) {
                commonPrefixLen = getCommonPrefixLen(keys[i-1].first, keys[i].first);
            } else {
                commonPrefixLen = getMax(getCommonPrefixLen(keys[i-1].first, keys[i].first),
                                         getCommonPrefixLen(keys[i].first, keys[i+1].first));
            }

            if (commonPrefixLen < (int)keys[i].first.length()) {
                keys_trunc.push_back({keys[i].first.substr(0, commonPrefixLen + 1),keys[i].second});
            } else {
                keys_trunc.push_back({keys[i].first,keys[i].second});
                keys_trunc[i].first += (char)kTerminator;
            }
        }
    }

static void fillinInts() {
    for (uint64_t i = 0; i < kIntTestSize; i += kIntTestSkip) {
        ints_.push_back({uint64ToString(i), i});
    }
}

static void loadWordList() {
    std::ifstream infile(kFilePath);
    std::string key;
    int count = 0;
    while (infile.good() && count < kTestSize) {
        infile >> key;
        words.push_back({key, count});
        words_dup.push_back({key, 2 * count});
        words_dup.push_back({key, 2 * count + 1});
        count++;
    }
    truncateSuffixes(words, words_trunc_);
    fillinInts();
    truncateSuffixes(ints_, ints_trunc_);
}

#endif // TESTSUITE_H_