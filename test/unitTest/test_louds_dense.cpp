#include "gtest/gtest.h"

#include <assert.h>

#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "louds_dense.hpp"
#include "surf_builder.hpp"

namespace surf {

    namespace densetest {

        static const std::string kFilePath = "../../../test/words.txt";
        static const int kTestSize = 234369;
        static const int kIntTestBound = 1000001;
        static const uint64_t kIntTestSkip = 10;
        static const bool kIncludeDense = true;
        static const uint32_t kSparseDenseRatio = 0;
        static const int kNumSuffixType = 4;
        static const SuffixType kSuffixTypeList[kNumSuffixType] = {kNone, kHash, kReal, kMixed};
        static const int kNumSuffixLen = 6;
        static const level_t kSuffixLenList[kNumSuffixLen] = {1, 3, 7, 8, 13, 26};
        static std::vector<std::pair<std::vector<label_t>,uint64_t>> words;

        class DenseUnitTest : public ::testing::Test {
        public:
            virtual void SetUp() {
                truncateSuffixes(words,words_trunc_);
                fillinInts();
                data_ = nullptr;
            }

            virtual void TearDown() {
                if (data_)
                    delete[] data_;
            }

            void newBuilder(SuffixType suffix_type, level_t suffix_len);

            void truncateSuffixes(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys,
                                  std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys_trunc);

            void fillinInts();

            void testSerialize();

            void testLookupWord();

            SuRFBuilder *builder_;
            LoudsDense *louds_dense_;
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_trunc_;
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_;
            char *data_;
        };

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

        void DenseUnitTest::newBuilder(SuffixType suffix_type, level_t suffix_len) {
            if (suffix_type == kNone)
                builder_ = new SuRFBuilder(kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
            else if (suffix_type == kHash)
                builder_ = new SuRFBuilder(kIncludeDense, kSparseDenseRatio, kHash, suffix_len, 0);
            else if (suffix_type == kReal)
                builder_ = new SuRFBuilder(kIncludeDense, kSparseDenseRatio, kReal, 0, suffix_len);
            else if (suffix_type == kMixed)
                builder_ = new SuRFBuilder(kIncludeDense, kSparseDenseRatio, kMixed, suffix_len, suffix_len);
            else
                builder_ = new SuRFBuilder(kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
        }

        void DenseUnitTest::truncateSuffixes(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys,
                                              std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys_trunc) {
            assert(words.size() > 1);

            int commonPrefixLen = 0;
            for (unsigned i = 0; i < keys.size(); i++) {
                if (i == 0) {
                    commonPrefixLen = getCommonPrefixLen(keys[i].first, keys[i + 1].first);
                } else if (i == keys.size() - 1) {
                    commonPrefixLen = getCommonPrefixLen(keys[i - 1].first, keys[i].first);
                } else {
                    commonPrefixLen = getMax(getCommonPrefixLen(keys[i - 1].first, keys[i].first),
                                             getCommonPrefixLen(keys[i].first, keys[i + 1].first));
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

        void DenseUnitTest::fillinInts() {
            for (uint64_t i = 0; i < kIntTestBound; i += kIntTestSkip) {
                ints_.push_back({uint64ToByteVector(i),i});
            }
        }

        void DenseUnitTest::testSerialize() {
            uint64_t size = louds_dense_->serializedSize();
            data_ = new char[size];
            LoudsDense *ori_louds_dense = louds_dense_;
            char *data = data_;
            ori_louds_dense->serialize(data);
            data = data_;
            louds_dense_ = LoudsDense::deSerialize(data);

            ASSERT_EQ(ori_louds_dense->getHeight(), louds_dense_->getHeight());

            ori_louds_dense->destroy();
            delete ori_louds_dense;
        }

        void DenseUnitTest::testLookupWord() {
            position_t out_node_num = 0;
            for (unsigned i = 0; i < words.size(); i++) {
                bool key_exist = louds_dense_->lookupKey(words[i].first, out_node_num).has_value();
                ASSERT_TRUE(key_exist);
            }

            for (unsigned i = 0; i < words.size(); i++) {
                for (unsigned j = 0; j < words_trunc_[i].first.size() && j < words[i].first.size(); j++) {
                    std::vector<label_t> key = words[i].first;
                    key[j] = 'A';
                    bool key_exist = louds_dense_->lookupKey(key, out_node_num).has_value();
                    ASSERT_FALSE(key_exist);
                }
            }
        }

        TEST_F (DenseUnitTest, lookupWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words);
                    louds_dense_ = new LoudsDense(builder_);
                    testLookupWord();
                    delete builder_;
                    louds_dense_->destroy();
                    delete louds_dense_;
                }
            }
        }

        /*TEST_F (DenseUnitTest, serializeTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words);
                    louds_dense_ = new LoudsDense(builder_);
                    testSerialize();
                    testLookupWord();
                    delete builder_;
                }
            }
        }*/

        TEST_F (DenseUnitTest, lookupIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_dense_ = new LoudsDense(builder_);
            position_t out_node_num = 0;

            for (uint64_t i = 0; i < kIntTestBound; i += kIntTestSkip) {
                bool key_exist = louds_dense_->lookupKey(uint64ToString(i), out_node_num).has_value();
                if (i % kIntTestSkip == 0) {
                    ASSERT_TRUE(key_exist);
                    ASSERT_EQ(0, out_node_num);
                } else {
                    ASSERT_FALSE(key_exist);
                    ASSERT_EQ(0, out_node_num);
                }
            }
            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        TEST_F (DenseUnitTest, moveToKeyGreaterThanWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words);
                    louds_dense_ = new LoudsDense(builder_);

                    bool inclusive = true;
                    for (int i = 0; i < 2; i++) {
                        if (i == 1)
                            inclusive = false;
                        for (unsigned j = 0; j < words.size() - 1; j++) {
                            LoudsDense::Iter iter(louds_dense_);
                            bool could_be_fp = louds_dense_->moveToKeyGreaterThan(words[j].first, inclusive, iter);

                            ASSERT_TRUE(iter.isValid());
                            ASSERT_TRUE(iter.isComplete());
                            std::vector<label_t> iter_key = iter.getKey();
                            bool is_prefix;
                            if (could_be_fp)
                                is_prefix = isSameKey(iter_key,words[j].first,iter_key.size());
                            else
                                is_prefix = isSameKey(iter_key,words[j + 1].first,iter_key.size());
                            ASSERT_TRUE(is_prefix);
                        }

                        LoudsDense::Iter iter(louds_dense_);
                        bool could_be_fp = louds_dense_->moveToKeyGreaterThan(words[words.size() - 1].first, inclusive, iter);
                        if (could_be_fp) {
                            std::vector<label_t> iter_key = iter.getKey();
                            bool is_prefix = isSameKey(iter_key,words[words.size() - 1].first,iter_key.size());
                            ASSERT_TRUE(is_prefix);
                        } else {
                            ASSERT_FALSE(iter.isValid());
                        }
                    }

                    delete builder_;
                    louds_dense_->destroy();
                    delete louds_dense_;
                }
            }
        }

        TEST_F (DenseUnitTest, moveToKeyGreaterThanIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_dense_ = new LoudsDense(builder_);

            bool inclusive = true;
            for (int i = 0; i < 2; i++) {
                if (i == 1)
                    inclusive = false;
                for (uint64_t j = 0; j < kIntTestBound; j++) {
                    LoudsDense::Iter iter(louds_dense_);
                    bool could_be_fp = louds_dense_->moveToKeyGreaterThan(uint64ToString(j), inclusive, iter);

                    ASSERT_TRUE(iter.isValid());
                    ASSERT_TRUE(iter.isComplete());
                    std::vector<label_t> iter_key = iter.getKey();
                    std::vector<label_t> int_key_fp = uint64ToByteVector(j - (j % kIntTestSkip));
                    std::vector<label_t> int_key_true = uint64ToByteVector(j - (j % kIntTestSkip) + kIntTestSkip);
                    bool is_prefix;
                    if (could_be_fp)
                        is_prefix = isSameKey(iter_key,int_key_fp,iter_key.size());
                    else
                        is_prefix = isSameKey(iter_key,int_key_true,iter_key.size());
                    ASSERT_TRUE(is_prefix);
                }

                LoudsDense::Iter iter(louds_dense_);
                bool could_be_fp = louds_dense_->moveToKeyGreaterThan(uint64ToString(kIntTestBound - 1), inclusive,
                                                                      iter);
                if (could_be_fp) {
                    std::vector<label_t> iter_key = iter.getKey();
                    std::vector<label_t> int_key_fp = uint64ToByteVector(kIntTestBound - 1);
                    bool is_prefix = isSameKey(iter_key,int_key_fp,iter_key.size());
                    ASSERT_TRUE(is_prefix);
                } else {
                    ASSERT_FALSE(iter.isValid());
                }
            }

            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        TEST_F (DenseUnitTest, IteratorIncrementWordTest) {
            newBuilder(kReal, 8);
            builder_->build(words);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(words[0].first, inclusive, iter);
            for (unsigned i = 1; i < words.size(); i++) {
                iter++;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,words[i].first,iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter++;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        TEST_F (DenseUnitTest, IteratorIncrementIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(uint64ToString(0), inclusive, iter);
            for (uint64_t i = kIntTestSkip; i < kIntTestBound; i += kIntTestSkip) {
                iter++;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                std::vector<label_t> int_prefix = uint64ToByteVector(i);
                bool is_prefix = isSameKey(iter_key,int_prefix,iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter++;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        TEST_F (DenseUnitTest, IteratorDecrementWordTest) {
            newBuilder(kReal, 8);
            builder_->build(words);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(words[words.size() - 1].first, inclusive, iter);
            for (int i = words.size() - 2; i >= 0; i--) {
                iter--;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,words[i].first,iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter--;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        TEST_F (DenseUnitTest, IteratorDecrementIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(uint64ToString(kIntTestBound - kIntTestSkip), inclusive, iter);
            for (uint64_t i = kIntTestBound - 1 - kIntTestSkip; i > 0; i -= kIntTestSkip) {
                iter--;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                std::vector<label_t> int_prefix = uint64ToByteVector(i);
                bool is_prefix = isSameKey(iter_key,int_prefix,iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter--;
            iter--;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        void loadWordList() {
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
                count++;
            }
        }

    } // namespace densetest

} // namespace surf

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    surf::densetest::loadWordList();
    return RUN_ALL_TESTS();
}
