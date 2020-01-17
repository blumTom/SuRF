#include "gtest/gtest.h"

#include <assert.h>

#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "louds_dense.hpp"
#include "surf_builder.hpp"
#include "testsuite.hpp"

namespace surf {

    namespace densetest {

        static const bool kIncludeDense = true;
        static const uint32_t kSparseDenseRatio = 0;
        static const int kNumSuffixType = 4;
        static const SuffixType kSuffixTypeList[kNumSuffixType] = {kNone, kHash, kReal, kMixed};
        static const int kNumSuffixLen = 6;
        static const level_t kSuffixLenList[kNumSuffixLen] = {1, 3, 7, 8, 13, 26};

        class DenseUnitTest : public ::testing::Test {
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

            SuRFBuilder<uint64_t> *builder_;
            LoudsDense<uint64_t> *louds_dense_;
            char *data_;
        };

        void DenseUnitTest::newBuilder(SuffixType suffix_type, level_t suffix_len) {
            if (suffix_type == kNone)
                builder_ = new SuRFBuilder<uint64_t>(kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
            else if (suffix_type == kHash)
                builder_ = new SuRFBuilder<uint64_t>(kIncludeDense, kSparseDenseRatio, kHash, suffix_len, 0);
            else if (suffix_type == kReal)
                builder_ = new SuRFBuilder<uint64_t>(kIncludeDense, kSparseDenseRatio, kReal, 0, suffix_len);
            else if (suffix_type == kMixed)
                builder_ = new SuRFBuilder<uint64_t>(kIncludeDense, kSparseDenseRatio, kMixed, suffix_len, suffix_len);
            else
                builder_ = new SuRFBuilder<uint64_t>(kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
        }

        void DenseUnitTest::testSerialize() {
            uint64_t size = louds_dense_->serializedSize();
            data_ = new char[size];
            LoudsDense<uint64_t> *ori_louds_dense = louds_dense_;
            char *data = data_;
            ori_louds_dense->serialize(data);
            data = data_;
            louds_dense_ = LoudsDense<uint64_t>::deSerialize(data);

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
                    std::string key = words[i].first;
                    key[j] = 'A';
                    bool key_exist = louds_dense_->lookupKey(key, out_node_num).has_value();
                    ASSERT_FALSE(key_exist);
                }
            }
        }

        TEST_F (DenseUnitTest, lookupWordTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_bytes;
            for (int i=0; i<words.size(); i++) {
                words_bytes.emplace_back(std::make_pair(stringToByteVector(words[i].first),words[i].second));
            }
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words_bytes);
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
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_bytes;
            for (int i=0; i<ints_.size(); i++) {
                ints_bytes.emplace_back(std::make_pair(stringToByteVector(ints_[i].first),ints_[i].second));
            }
            newBuilder(kReal, 8);
            builder_->build(ints_bytes);
            louds_dense_ = new LoudsDense(builder_);
            position_t out_node_num = 0;

            for (uint64_t i = 0; i < kIntTestSize; i += kIntTestSkip) {
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
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_bytes;
            for (int i=0; i<words.size(); i++) {
                words_bytes.emplace_back(std::make_pair(stringToByteVector(words[i].first),words[i].second));
            }
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newBuilder(kSuffixTypeList[t], kSuffixLenList[k]);
                    builder_->build(words_bytes);
                    louds_dense_ = new LoudsDense(builder_);

                    bool inclusive = true;
                    for (int i = 0; i < 2; i++) {
                        if (i == 1)
                            inclusive = false;
                        for (unsigned j = 0; j < words.size() - 1; j++) {
                            LoudsDense<uint64_t>::Iter iter(louds_dense_);
                            bool could_be_fp = louds_dense_->moveToKeyGreaterThan(words[j].first, inclusive, iter);

                            ASSERT_TRUE(iter.isValid());
                            ASSERT_TRUE(iter.isComplete());
                            std::vector<label_t> iter_key = iter.getKey();
                            bool is_prefix;
                            if (could_be_fp)
                                is_prefix = isSameKey(iter_key,stringToByteVector(words[j].first),iter_key.size());
                            else
                                is_prefix = isSameKey(iter_key,stringToByteVector(words[j + 1].first),iter_key.size());
                            ASSERT_TRUE(is_prefix);
                        }

                        LoudsDense<uint64_t>::Iter iter(louds_dense_);
                        bool could_be_fp = louds_dense_->moveToKeyGreaterThan(words[words.size() - 1].first, inclusive, iter);
                        if (could_be_fp) {
                            std::vector<label_t> iter_key = iter.getKey();
                            bool is_prefix = isSameKey(iter_key,stringToByteVector(words[words.size() - 1].first),iter_key.size());
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
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_bytes;
            for (int i=0; i<ints_.size(); i++) {
                ints_bytes.emplace_back(std::make_pair(stringToByteVector(ints_[i].first),ints_[i].second));
            }

            newBuilder(kReal, 8);
            builder_->build(ints_bytes);
            louds_dense_ = new LoudsDense(builder_);

            bool inclusive = true;
            for (int i = 0; i < 2; i++) {
                if (i == 1)
                    inclusive = false;
                for (uint64_t j = 0; j < kIntTestSize; j++) {
                    LoudsDense<uint64_t>::Iter iter(louds_dense_);
                    bool could_be_fp = louds_dense_->moveToKeyGreaterThan(uint64ToString(j), inclusive, iter);

                    ASSERT_TRUE(iter.isValid());
                    ASSERT_TRUE(iter.isComplete());
                    std::vector<label_t> iter_key = iter.getKey();
                    std::vector<label_t> int_key_fp = stringToByteVector(uint64ToString(j - (j % kIntTestSkip)));
                    std::vector<label_t> int_key_true = stringToByteVector(uint64ToString(j - (j % kIntTestSkip) + kIntTestSkip));
                    bool is_prefix;
                    if (could_be_fp)
                        is_prefix = isSameKey(iter_key,int_key_fp,iter_key.size());
                    else
                        is_prefix = isSameKey(iter_key,int_key_true,iter_key.size());
                    ASSERT_TRUE(is_prefix);
                }

                LoudsDense<uint64_t>::Iter iter(louds_dense_);
                bool could_be_fp = louds_dense_->moveToKeyGreaterThan(uint64ToString(kIntTestSize - 1), inclusive,
                                                                      iter);
                if (could_be_fp) {
                    std::vector<label_t> iter_key = iter.getKey();
                    std::vector<label_t> int_key_fp = stringToByteVector(uint64ToString(kIntTestSize - 1));
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
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_bytes;
            for (int i=0; i<words.size(); i++) {
                words_bytes.emplace_back(std::make_pair(stringToByteVector(words[i].first),words[i].second));
            }
            newBuilder(kReal, 8);
            builder_->build(words_bytes);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense<uint64_t>::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(words[0].first, inclusive, iter);
            for (unsigned i = 1; i < words.size(); i++) {
                iter++;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,stringToByteVector(words[i].first),iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter++;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        TEST_F (DenseUnitTest, IteratorIncrementIntTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_bytes;
            for (int i=0; i<ints_.size(); i++) {
                ints_bytes.emplace_back(std::make_pair(stringToByteVector(ints_[i].first),ints_[i].second));
            }
            newBuilder(kReal, 8);
            builder_->build(ints_bytes);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense<uint64_t>::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(uint64ToString(0), inclusive, iter);
            for (uint64_t i = kIntTestSkip; i < kIntTestSize; i += kIntTestSkip) {
                iter++;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                std::vector<label_t> int_prefix = stringToByteVector(uint64ToString(i));
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
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_bytes;
            for (int i=0; i<words.size(); i++) {
                words_bytes.emplace_back(std::make_pair(stringToByteVector(words[i].first),words[i].second));
            }
            newBuilder(kReal, 8);
            builder_->build(words_bytes);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense<uint64_t>::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(words[words.size() - 1].first, inclusive, iter);
            for (int i = words.size() - 2; i >= 0; i--) {
                iter--;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                bool is_prefix = isSameKey(iter_key,stringToByteVector(words[i].first),iter_key.size());
                ASSERT_TRUE(is_prefix);
            }
            iter--;
            ASSERT_FALSE(iter.isValid());
            delete builder_;
            louds_dense_->destroy();
            delete louds_dense_;
        }

        TEST_F (DenseUnitTest, IteratorDecrementIntTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_bytes;
            for (int i=0; i<ints_.size(); i++) {
                ints_bytes.emplace_back(std::make_pair(stringToByteVector(ints_[i].first),ints_[i].second));
            }
            newBuilder(kReal, 8);
            builder_->build(ints_bytes);
            louds_dense_ = new LoudsDense(builder_);
            bool inclusive = true;
            LoudsDense<uint64_t>::Iter iter(louds_dense_);
            louds_dense_->moveToKeyGreaterThan(uint64ToString(kIntTestSize - kIntTestSkip), inclusive, iter);
            for (uint64_t i = kIntTestSize - 1 - kIntTestSkip; i > 0; i -= kIntTestSkip) {
                iter--;
                ASSERT_TRUE(iter.isValid());
                ASSERT_TRUE(iter.isComplete());
                std::vector<label_t> iter_key = iter.getKey();
                std::vector<label_t> int_prefix = stringToByteVector(uint64ToString(i));
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

    } // namespace densetest

} // namespace surf
