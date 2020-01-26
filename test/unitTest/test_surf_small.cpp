#include "gtest/gtest.h"

#include <assert.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "surf.hpp"

namespace surf {

    namespace surftest {

        static const SuffixType kSuffixType = kReal;
        static const level_t kSuffixLen = 8;

        class SuRFSmallTest : public ::testing::Test {
        public:
            virtual void SetUp() {}

            virtual void TearDown() {}
        };

        TEST_F (SuRFSmallTest, ExampleInPaperTest) {
            std::vector<std::pair<std::string,uint64_t>> keys;

            keys.push_back({std::string("f"),0});
            keys.push_back({std::string("far"),1});
            keys.push_back({std::string("fas"),2});
            keys.push_back({std::string("fast"),3});
            keys.push_back({std::string("fat"),4});
            keys.push_back({std::string("s"),5});
            keys.push_back({std::string("top"),6});
            keys.push_back({std::string("toy"),7});
            keys.push_back({std::string("trie"),8});
            keys.push_back({std::string("trip"),9});
            keys.push_back({std::string("try"),10});

            SuRF<std::string,uint64_t> *surf = new SuRF<std::string,uint64_t>(keys, kIncludeDense, kSparseDenseRatio, kSuffixType, 0, kSuffixLen,stringToByteVector);
            bool exist = surf->lookupRange(std::string("top"), false, std::string("toyy"), false).size() > 0;
            ASSERT_TRUE(exist);
            exist = surf->lookupRange(std::string("toq"), false, std::string("toyy"), false).size() > 0;
            ASSERT_TRUE(exist);
            exist = surf->lookupRange(std::string("trie"), false, std::string("tripp"), false).size() > 0;
            ASSERT_TRUE(exist);

            SuRF<std::string,uint64_t>::Iter iter = surf->moveToKeyGreaterThan(std::string("t"), true);
            ASSERT_TRUE(iter.isValid());
            iter++;
            ASSERT_TRUE(iter.isValid());
        }

        TEST_F (SuRFSmallTest, InsertTest) {
            std::vector<std::pair<std::string,uint64_t>> keys;

            keys.push_back({std::string("f"),0});
            keys.push_back({std::string("far"),1});
            keys.push_back({std::string("fas"),2});
            keys.push_back({std::string("fast"),3});
            keys.push_back({std::string("fat"),4});
            keys.push_back({std::string("s"),5});
            keys.push_back({std::string("top"),6});
            keys.push_back({std::string("toy"),7});
            keys.push_back({std::string("trie"),8});
            keys.push_back({std::string("trip"),9});
            keys.push_back({std::string("try"),10});

            std::vector<std::pair<std::string,uint64_t>> insertKeys;

            insertKeys.push_back({std::string("aa"),11});
            insertKeys.push_back({std::string("ab"),12});
            insertKeys.push_back({std::string("ba"),13});
            insertKeys.push_back({std::string("bba"),14});
            insertKeys.push_back({std::string("bbba"),15});
            insertKeys.push_back({std::string("bbbb"),16});
            insertKeys.push_back({std::string("xxxy"),17});
            insertKeys.push_back({std::string("xyz"),18});
            insertKeys.push_back({std::string("hey"),19});
            insertKeys.push_back({std::string("hi"),20});
            insertKeys.push_back({std::string("a"),21});
            insertKeys.push_back({std::string("xxxz"),22});

            std::function<std::vector<label_t>(const uint64_t)> keyFromValueDerivator = [&keys](const uint64_t& value) { return stringToByteVector(keys[value].first); };

            SuRF<std::string,uint64_t> *surf = new SuRF<std::string,uint64_t>(keys, kIncludeDense, kSparseDenseRatio, kSuffixType, 0, kSuffixLen,stringToByteVector);
            for (auto& key : keys) {
                ASSERT_EQ(surf->lookupKey(key.first),key.second);
            }
            for (auto& key : insertKeys) {
                surf->insert(key.first,key.second,keyFromValueDerivator);
                keys.push_back(key);
                std::optional<uint64_t> result = surf->lookupKey(key.first);
                ASSERT_TRUE(result.has_value());
                ASSERT_EQ(result.value(),key.second);
            }
            for (auto& key : keys) {
                ASSERT_EQ(surf->lookupKey(key.first),key.second);
            }
        }

    } // namespace surftest

} // namespace surf