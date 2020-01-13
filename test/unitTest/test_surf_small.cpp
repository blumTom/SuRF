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

            SuRF *surf = new SuRF(keys, kIncludeDense, kSparseDenseRatio, kSuffixType, 0, kSuffixLen);
            bool exist = surf->lookupRange(std::string("top"), false, std::string("toyy"), false).size() > 0;
            ASSERT_TRUE(exist);
            exist = surf->lookupRange(std::string("toq"), false, std::string("toyy"), false).size() > 0;
            ASSERT_TRUE(exist);
            exist = surf->lookupRange(std::string("trie"), false, std::string("tripp"), false).size() > 0;
            ASSERT_TRUE(exist);

            SuRF::Iter iter = surf->moveToKeyGreaterThan(std::string("t"), true);
            ASSERT_TRUE(iter.isValid());
            iter++;
            ASSERT_TRUE(iter.isValid());
        }

    } // namespace surftest

} // namespace surf