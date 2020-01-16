#include "gtest/gtest.h"

#include "testsuite.hpp"

std::vector<std::pair<std::string,uint64_t>> words;
std::vector<std::pair<std::string,uint64_t>> words_dup;
std::vector<std::pair<std::string,uint64_t>> ints_;
std::vector<std::pair<std::string,uint64_t>> words_trunc_;
std::vector<std::pair<std::string,uint64_t>> ints_trunc_;

int main(int argc, char *argv[]) {
    testing::InitGoogleTest(&argc, argv);
    loadWordList();
    return RUN_ALL_TESTS();
}