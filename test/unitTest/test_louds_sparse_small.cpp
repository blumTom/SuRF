#include "gtest/gtest.h"

#include <assert.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "surf.hpp"

namespace surf {

    namespace surftest {

        static const bool kIncludeDense = false;
        static const uint32_t kSparseDenseRatio = 0;
        static const SuffixType kSuffixType = kReal;
        static const level_t kSuffixLen = 8;

        class SuRFSparseSmallTest : public ::testing::Test {
        public:
            virtual void SetUp() {}

            virtual void TearDown() {}
        };

        TEST_F (SuRFSparseSmallTest, ExampleInPaperTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keys;

            keys.push_back({stringToByteVector(std::string("f")),0});
            keys.push_back({stringToByteVector(std::string("far")),1});
            keys.push_back({stringToByteVector(std::string("fas")),2});
            keys.push_back({stringToByteVector(std::string("fast")),3});
            keys.push_back({stringToByteVector(std::string("fat")),4});
            keys.push_back({stringToByteVector(std::string("s")),5});
            keys.push_back({stringToByteVector(std::string("top")),6});
            keys.push_back({stringToByteVector(std::string("toy")),7});
            keys.push_back({stringToByteVector(std::string("trie")),8});
            keys.push_back({stringToByteVector(std::string("trip")),9});
            keys.push_back({stringToByteVector(std::string("try")),10});

            SuRFBuilder<uint64_t> *builder = new SuRFBuilder<uint64_t>(kIncludeDense, kSparseDenseRatio, kSuffixType, 0, kSuffixLen);
            builder->build(keys);
            LoudsSparse<uint64_t> *louds_sparse = new LoudsSparse(builder);
            LoudsSparse<uint64_t>::Iter iter(louds_sparse);

            louds_sparse->moveToKeyGreaterThan(std::string("to"), true, iter);
            ASSERT_TRUE(iter.isValid());
            ASSERT_TRUE(isSameKey(iter.getKey(),stringToByteVector("top"),std::max(iter.getKey().size(),stringToByteVector("top").size())));
            iter++;
            ASSERT_TRUE(isSameKey(iter.getKey(),stringToByteVector("toy"),std::max(iter.getKey().size(),stringToByteVector("toy").size())));
        }

    } // namespace surftest

} // namespace surf