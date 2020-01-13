#include "gtest/gtest.h"

#include <assert.h>

#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "louds_sparse.hpp"
#include "surf_builder.hpp"
#include "testsuite.hpp"

namespace surf {

    namespace sparsetest {

        static const bool kIncludeDense = false;
        static const uint32_t kSparseDenseRatio = 0;
        static const int kNumSuffixType = 4;
        static const SuffixType kSuffixTypeList[kNumSuffixType] = {kNone, kHash, kReal, kMixed};
        static const int kNumSuffixLen = 6;
        static const level_t kSuffixLenList[kNumSuffixLen] = {1, 3, 7, 8, 13, 26};

        class SparseUnitTest : public ::testing::Test {
        public:
            virtual void SetUp() {
                data_ = nullptr;
            }

            virtual void TearDown() {
                if (data_)
                    delete[] data_;
            }

            void newBuilder(SuffixType suffix_type, level_t suffix_len);

            void testSerialize();

            void testLookupWord();

            SuRFBuilder *builder_;
            LoudsSparse *louds_sparse_;
            char *data_;
        };

        void SparseUnitTest::newBuilder(SuffixType suffix_type, level_t suffix_len) {
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

        void SparseUnitTest::testSerialize() {
            uint64_t size = louds_sparse_->serializedSize();
            data_ = new char[size];
            LoudsSparse *ori_louds_sparse = louds_sparse_;
            char *data = data_;
            ori_louds_sparse->serialize(data);
            data = data_;
            louds_sparse_ = LoudsSparse::deSerialize(data);

            ASSERT_EQ(ori_louds_sparse->getHeight(), louds_sparse_->getHeight());
            ASSERT_EQ(ori_louds_sparse->getStartLevel(), louds_sparse_->getStartLevel());

            ori_louds_sparse->destroy();
            delete ori_louds_sparse;
        }

        void SparseUnitTest::testLookupWord() {
            position_t in_node_num = 0;
            for (unsigned i = 0; i < words.size(); i++) {
                bool key_exist = louds_sparse_->lookupKey(words[i].first, in_node_num).has_value();
                ASSERT_TRUE(key_exist);
            }

            for (unsigned i = 0; i < words.size(); i++) {
                for (unsigned j = 0; j < words_trunc_[i].first.size() && j < words[i].first.size(); j++) {
                    std::vector<label_t> key = words[i].first;
                    key[j] = 'A';
                    bool key_exist = louds_sparse_->lookupKey(key, in_node_num).has_value();
                    ASSERT_FALSE(key_exist);
                }
            }
        }

        TEST_F (SparseUnitTest, lookupWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words);
                    louds_sparse_ = new LoudsSparse(builder_);

                    testLookupWord();
                    delete builder_;
                    louds_sparse_->destroy();
                    delete louds_sparse_;
                }
            }
        }

        /*TEST_F (SparseUnitTest, serializeTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words);
                    louds_sparse_ = new LoudsSparse(builder_);

                    testSerialize();
                    testLookupWord();
                    delete builder_;
                }
            }
        }*/

        TEST_F (SparseUnitTest, lookupIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_sparse_ = new LoudsSparse(builder_);
            position_t in_node_num = 0;

            for (uint64_t i = 0; i < kIntTestSize; i += kIntTestSkip) {
                bool key_exist = louds_sparse_->lookupKey(uint64ToString(i), in_node_num).has_value();
                if (i % kIntTestSkip == 0)
                    ASSERT_TRUE(key_exist);
                else
                    ASSERT_FALSE(key_exist);
            }
            delete builder_;
            louds_sparse_->destroy();
            delete louds_sparse_;
        }

        TEST_F (SparseUnitTest, moveToKeyGreaterThanWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words);
                    louds_sparse_ = new LoudsSparse(builder_);

                    bool inclusive = true;
                    for (int i = 0; i < 2; i++) {
                        if (i == 1)
                            inclusive = false;
                        for (unsigned j = 0; j < words.size() - 1; j++) {
                            LoudsSparse::Iter iter(louds_sparse_);
                            bool could_be_fp = louds_sparse_->moveToKeyGreaterThan(words[j].first, inclusive, iter);

                            ASSERT_TRUE(iter.isValid());
                            std::vector<label_t> iter_key = iter.getKey();
                            bool is_prefix;
                            if (could_be_fp)
                                is_prefix = isSameKey(iter_key,words[j].first,iter_key.size());
                            else
                                is_prefix = isSameKey(iter_key,words[j + 1].first,iter_key.size());
                            ASSERT_TRUE(is_prefix);
                        }

                        LoudsSparse::Iter iter(louds_sparse_);
                        bool could_be_fp = louds_sparse_->moveToKeyGreaterThan(words[words.size() - 1].first, inclusive,
                                                                               iter);
                        if (could_be_fp) {
                            std::vector<label_t> iter_key = iter.getKey();
                            bool is_prefix = isSameKey(iter_key,words[words.size() - 1].first,iter_key.size());
                            ASSERT_TRUE(is_prefix);
                        } else {
                            ASSERT_FALSE(iter.isValid());
                        }
                    }

                    delete builder_;
                    louds_sparse_->destroy();
                    delete louds_sparse_;
                }
            }
        }

        TEST_F (SparseUnitTest, moveToKeyGreaterThanIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_sparse_ = new LoudsSparse(builder_);

            bool inclusive = true;
            for (int i = 0; i < 2; i++) {
                if (i == 1)
                    inclusive = false;
                for (uint64_t j = 0; j < kIntTestSize - 1; j++) {
                    LoudsSparse::Iter iter(louds_sparse_);
                    bool could_be_fp = louds_sparse_->moveToKeyGreaterThan(uint64ToString(j), inclusive, iter);

                    ASSERT_TRUE(iter.isValid());
                    std::vector<label_t> iter_key = iter.getKey();
                    std::vector<label_t> int_key_fp = uint64ToByteVector(j - (j % kIntTestSkip));
                    std::vector<label_t> int_key_true = uint64ToByteVector(j - (j % kIntTestSkip) + kIntTestSkip);
                    bool is_prefix = false;
                    if (could_be_fp)
                        is_prefix = isSameKey(iter_key,int_key_fp,iter_key.size());
                    else
                        is_prefix = isSameKey(iter_key,int_key_true,iter_key.size());
                    ASSERT_TRUE(is_prefix);
                }

                LoudsSparse::Iter iter(louds_sparse_);
                bool could_be_fp = louds_sparse_->moveToKeyGreaterThan(uint64ToString(kIntTestSize - 1), inclusive,
                                                                       iter);
                if (could_be_fp) {
                    std::vector<label_t> iter_key = iter.getKey();
                    std::vector<label_t> int_key_fp = uint64ToByteVector(kIntTestSize - 1);
                    bool is_prefix = isSameKey(iter_key,int_key_fp,iter_key.size());
                    ASSERT_TRUE(is_prefix);
                } else {
                    ASSERT_FALSE(iter.isValid());
                }
            }

            delete builder_;
            louds_sparse_->destroy();
            delete louds_sparse_;
        }

        TEST_F (SparseUnitTest, IteratorIncrementWordTest) {
            newBuilder(kReal, 8);
            builder_->build(words);
            louds_sparse_ = new LoudsSparse(builder_);
            bool inclusive = true;
            LoudsSparse::Iter iter(louds_sparse_);
            louds_sparse_->moveToKeyGreaterThan(words[0].first, inclusive, iter);
            for (unsigned i = 1; i < words.size(); i++) {
                iter++;
                ASSERT_TRUE(iter.isValid());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,words[i].first,iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter++;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_sparse_->destroy();
            delete louds_sparse_;
        }

        TEST_F (SparseUnitTest, IteratorIncrementIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_sparse_ = new LoudsSparse(builder_);
            bool inclusive = true;
            LoudsSparse::Iter iter(louds_sparse_);
            louds_sparse_->moveToKeyGreaterThan(uint64ToString(0), inclusive, iter);
            for (uint64_t i = kIntTestSkip; i < kIntTestSize; i += kIntTestSkip) {
                iter++;
                ASSERT_TRUE(iter.isValid());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,uint64ToByteVector(i),iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter++;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_sparse_->destroy();
            delete louds_sparse_;
        }

        TEST_F (SparseUnitTest, IteratorDecrementWordTest) {
            newBuilder(kReal, 8);
            builder_->build(words);
            louds_sparse_ = new LoudsSparse(builder_);
            bool inclusive = true;
            LoudsSparse::Iter iter(louds_sparse_);
            louds_sparse_->moveToKeyGreaterThan(words[words.size() - 1].first, inclusive, iter);
            for (int i = words.size() - 2; i >= 0; i--) {
                iter--;
                ASSERT_TRUE(iter.isValid());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,words[i].first,iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter--;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_sparse_->destroy();
            delete louds_sparse_;
        }

        TEST_F (SparseUnitTest, IteratorDecrementIntTest) {
            newBuilder(kReal, 8);
            builder_->build(ints_);
            louds_sparse_ = new LoudsSparse(builder_);
            bool inclusive = true;
            LoudsSparse::Iter iter(louds_sparse_);
            louds_sparse_->moveToKeyGreaterThan(uint64ToString(kIntTestSize - kIntTestSkip), inclusive, iter);
            for (uint64_t i = kIntTestSize - 1 - kIntTestSkip; i > 0; i -= kIntTestSkip) {
                iter--;
                ASSERT_TRUE(iter.isValid());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,uint64ToByteVector(i),iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter--;
            iter--;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_sparse_->destroy();
            delete louds_sparse_;
        }

        TEST_F (SparseUnitTest, FirstAndLastLabelInRootTest) {
            newBuilder(kReal, 8);
            builder_->build(words);
            louds_sparse_ = new LoudsSparse(builder_);
            LoudsSparse::Iter iter(louds_sparse_);
            iter.setToFirstLabelInRoot();
            iter.moveToLeftMostKey();
            std::vector<label_t> iter_key = iter.getKey();
            bool is_prefix = isSameKey(iter_key,words[0].first,iter_key.size());
            ASSERT_TRUE(is_prefix);

            iter.clear();
            iter.setToLastLabelInRoot();
            iter.moveToRightMostKey();
            iter_key = iter.getKey();
            is_prefix = isSameKey(iter_key,words[kTestSize - 1].first,iter_key.size());
            ASSERT_TRUE(is_prefix);

            delete builder_;
            louds_sparse_->destroy();
            delete louds_sparse_;
        }

    } // namespace sparsetest

} // namespace surf
