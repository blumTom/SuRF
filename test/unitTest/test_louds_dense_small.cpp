#include "gtest/gtest.h"

#include <assert.h>

#include <string>
#include <vector>

#include "config.hpp"
#include "surf.hpp"

namespace surf {

    namespace surftest {

        static const bool kIncludeDense = true;
        static const uint32_t kSparseDenseRatio = 0;
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

            SuRFBuilder *builder = new SuRFBuilder(kIncludeDense, kSparseDenseRatio, kSuffixType, 0, kSuffixLen);
            builder->build(keys);
            LoudsDense *louds_dense = new LoudsDense(builder);
            LoudsDense::Iter iter(louds_dense);

            louds_dense->moveToKeyGreaterThan(std::string("to"), true, iter);
            ASSERT_TRUE(iter.isValid());
            ASSERT_EQ(0, iter.getKey().compare("top"));
            iter++;
            ASSERT_EQ(0, iter.getKey().compare("toy"));

            iter.clear();
            louds_dense->moveToKeyGreaterThan(std::string("fas"), true, iter);
            ASSERT_TRUE(iter.isValid());
            ASSERT_EQ(0, iter.getKey().compare("fas"));
        }

    } // namespace surftest

} // namespace surf

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
