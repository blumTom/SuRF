#include "gtest/gtest.h"

#include <assert.h>

#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "surf_builder.hpp"
#include "testsuite.hpp"

namespace surf {

    namespace buildertest {

        class SuRFBuilderUnitTest : public ::testing::Test {
        public:
            virtual void SetUp() {

            }

            bool DoesPrefixMatchInTrunc(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys_trunc,
                                        int i, int j, int len);

            void testSparse(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys,
                            const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys_trunc);

            void testDense();

            //debug
            void printDenseNode(level_t level, position_t node_num);

            void printSparseNode(level_t level, position_t pos);

            SuRFBuilder<uint64_t> *builder_;
        };

        void printIndent(level_t level) {
            for (level_t l = 0; l < level; l++)
                std::cout << "\t";
        }

        void SuRFBuilderUnitTest::printDenseNode(level_t level, position_t node_num) {
            printIndent(level);
            std::cout << "level = " << level << "\tnode_num = " << node_num << "\n";

            // print labels
            printIndent(level);
            for (position_t i = 0; i < kFanout; i++) {
                if (SuRFBuilder<uint64_t>::readBit(builder_->getBitmapLabels()[level], node_num * kFanout + i)) {
                    if ((i >= 65 && i <= 90) || (i >= 97 && i <= 122))
                        std::cout << (char) i << " ";
                    else
                        std::cout << (int16_t) i << " ";
                }
            }
            std::cout << "\n";

            // print child indicator bitmap
            printIndent(level);
            for (position_t i = 0; i < kFanout; i++) {
                if (SuRFBuilder<uint64_t>::readBit(builder_->getBitmapLabels()[level], node_num * kFanout + i)) {
                    if (SuRFBuilder<uint64_t>::readBit(builder_->getBitmapChildIndicatorBits()[level], node_num * kFanout + i))
                        std::cout << "1 ";
                    else
                        std::cout << "0 ";
                }
            }
            std::cout << "\n";

            // print prefixkey indicator
            printIndent(level);
            if (SuRFBuilder<uint64_t>::readBit(builder_->getPrefixkeyIndicatorBits()[level], node_num))
                std::cout << "1 ";
            else
                std::cout << "0 ";
            std::cout << "\n";
        }

        void SuRFBuilderUnitTest::printSparseNode(level_t level, position_t pos) {
            printIndent(level);
            std::cout << "level = " << level << "\tpos = " << pos << "\n";

            position_t start_pos = pos;

            // print labels
            printIndent(level);
            bool is_end_of_node = false;
            while (!is_end_of_node && pos < builder_->getLabels()[level].size()) {
                label_t label = builder_->getLabels()[level][pos];
                if ((label >= 65 && label <= 90) || (label >= 97 && label <= 122))
                    std::cout << (char) label << " ";
                else
                    std::cout << (int16_t) label << " ";
                pos++;
                is_end_of_node = SuRFBuilder<uint64_t>::readBit(builder_->getLoudsBits()[level], pos);
            }
            std::cout << "\n";

            // print child indicators
            printIndent(level);
            is_end_of_node = false;
            pos = start_pos;
            while (!is_end_of_node && pos < builder_->getLabels()[level].size()) {
                bool has_child = SuRFBuilder<uint64_t>::readBit(builder_->getChildIndicatorBits()[level], pos);
                if (has_child)
                    std::cout << "1 ";
                else
                    std::cout << "0 ";
                pos++;
                is_end_of_node = SuRFBuilder<uint64_t>::readBit(builder_->getLoudsBits()[level], pos);
            }
            std::cout << "\n";

            // print louds bits
            printIndent(level);
            is_end_of_node = false;
            pos = start_pos;
            while (!is_end_of_node && pos < builder_->getLabels()[level].size()) {
                bool louds_bit = SuRFBuilder<uint64_t>::readBit(builder_->getLoudsBits()[level], pos);
                if (louds_bit)
                    std::cout << "1 ";
                else
                    std::cout << "0 ";
                pos++;
                is_end_of_node = SuRFBuilder<uint64_t>::readBit(builder_->getLoudsBits()[level], pos);
            }
            std::cout << "\n";
        }

        bool
        SuRFBuilderUnitTest::DoesPrefixMatchInTrunc(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys_trunc, int i, int j, int len) {
            if (i < 0 || i >= (int) keys_trunc.size()) return false;
            if (j < 0 || j >= (int) keys_trunc.size()) return false;
            if (len <= 0) return true;
            if ((int) keys_trunc[i].first.size() < len) return false;
            if ((int) keys_trunc[j].first.size() < len) return false;

            if (isSameKey(keys_trunc[i].first,keys_trunc[j].first,len)) return true;
            return false;
        }

        void SuRFBuilderUnitTest::testSparse(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys,
                                             const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys_trunc) {
            for (level_t level = 0; level < builder_->getTreeHeight(); level++) {
                position_t pos = 0;
                pos--;
                position_t suffix_bitpos = 0;
                for (int i = 0; i < (int) keys_trunc.size(); i++) {
                    if (level >= keys_trunc[i].first.size())
                        continue;
                    if (DoesPrefixMatchInTrunc(keys_trunc, i - 1, i, level + 1))
                        continue;
                    pos++;

                    // label test
                    label_t label = (label_t) keys_trunc[i].first[level];
                    bool exist_in_node = (builder_->getLabels()[level][pos] == label);
                    ASSERT_TRUE(exist_in_node);

                    // child indicator test
                    bool has_child = SuRFBuilder<uint64_t>::readBit(builder_->getChildIndicatorBits()[level], pos);
                    bool same_prefix_in_prev_key = DoesPrefixMatchInTrunc(keys_trunc, i - 1, i, level + 1);
                    bool same_prefix_in_next_key = DoesPrefixMatchInTrunc(keys_trunc, i, i + 1, level + 1);
                    bool expected_has_child = same_prefix_in_prev_key || same_prefix_in_next_key;
                    ASSERT_EQ(expected_has_child, has_child);

                    // LOUDS bit test
                    bool louds_bit = SuRFBuilder<uint64_t>::readBit(builder_->getLoudsBits()[level], pos);
                    bool expected_louds_bit = !DoesPrefixMatchInTrunc(keys_trunc, i - 1, i, level);
                    if (pos == 0)
                        ASSERT_TRUE(louds_bit);
                    else
                        ASSERT_EQ(expected_louds_bit, louds_bit);

                    // suffix test
                    if (!has_child) {
                        position_t suffix_len = builder_->getSuffixLen();
                        if (((keys[i].first.size() - level - 1) * 8) >= suffix_len) {
                            for (position_t bitpos = 0; bitpos < suffix_len; bitpos++) {
                                position_t byte_id = bitpos / 8;
                                position_t byte_offset = bitpos % 8;
                                uint8_t byte_mask = 0x80;
                                byte_mask >>= byte_offset;
                                bool expected_suffix_bit = false;
                                if (level + 1 + byte_id < keys[i].first.size())
                                    expected_suffix_bit = (bool) (keys[i].first[level + 1 + byte_id] & byte_mask);
                                bool stored_suffix_bit = SuRFBuilder<uint64_t>::readBit(builder_->getSuffixes()[level],
                                                                              suffix_bitpos);
                                ASSERT_EQ(expected_suffix_bit, stored_suffix_bit);
                                suffix_bitpos++;
                            }
                        } else {
                            for (position_t bitpos = 0; bitpos < suffix_len; bitpos++) {
                                bool stored_suffix_bit = SuRFBuilder<uint64_t>::readBit(builder_->getSuffixes()[level],
                                                                              suffix_bitpos);
                                ASSERT_FALSE(stored_suffix_bit);
                                suffix_bitpos++;
                            }
                        }
                    }
                }
            }
        }

        void SuRFBuilderUnitTest::testDense() {
            for (level_t level = 0; level < builder_->getSparseStartLevel(); level++) {
                int node_num = -1;

                label_t prev_label = 0;
                for (unsigned i = 0; i < builder_->getLabels()[level].size(); i++) {
                    bool is_node_start = SuRFBuilder<uint64_t>::readBit(builder_->getLoudsBits()[level], i);
                    if (is_node_start)
                        node_num++;

                    label_t label = builder_->getLabels()[level][i];
                    bool exist_in_node = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapLabels()[level],
                                                              node_num * kFanout + label);
                    bool has_child_sparse = SuRFBuilder<uint64_t>::readBit(builder_->getChildIndicatorBits()[level], i);
                    bool has_child_dense = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapChildIndicatorBits()[level],
                                                                node_num * kFanout + label);

                    // prefixkey indicator test
                    if (is_node_start) {
                        bool prefixkey_indicator = SuRFBuilder<uint64_t>::readBit(builder_->getPrefixkeyIndicatorBits()[level],
                                                                        node_num);
                        if ((label == kTerminator) && !has_child_sparse)
                            ASSERT_TRUE(prefixkey_indicator);
                        else
                            ASSERT_FALSE(prefixkey_indicator);
                        prev_label = label;
                        continue;
                    }

                    // label bitmap test
                    ASSERT_TRUE(exist_in_node);

                    // child indicator bitmap test
                    ASSERT_EQ(has_child_sparse, has_child_dense);

                    // label, child indicator bitmap zero bit test
                    if (is_node_start) {
                        if (node_num > 0) {
                            for (unsigned c = prev_label + 1; c < kFanout; c++) {
                                exist_in_node = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapLabels()[level],
                                                                     (node_num - 1) * kFanout + c);
                                ASSERT_FALSE(exist_in_node);
                                has_child_dense = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapChildIndicatorBits()[level],
                                                                       (node_num - 1) * kFanout + c);
                                ASSERT_FALSE(has_child_dense);
                            }
                        }
                        for (unsigned c = 0; c < (unsigned) label; c++) {
                            exist_in_node = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapLabels()[level],
                                                                 node_num * kFanout + c);
                            ASSERT_FALSE(exist_in_node);
                            has_child_dense = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapChildIndicatorBits()[level],
                                                                   node_num * kFanout + c);
                            ASSERT_FALSE(has_child_dense);
                        }
                    } else {
                        for (unsigned c = prev_label + 1; c < (unsigned) label; c++) {
                            exist_in_node = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapLabels()[level],
                                                                 node_num * kFanout + c);
                            ASSERT_FALSE(exist_in_node);
                            has_child_dense = SuRFBuilder<uint64_t>::readBit(builder_->getBitmapChildIndicatorBits()[level],
                                                                   node_num * kFanout + c);
                            ASSERT_FALSE(has_child_dense);
                        }
                    }
                    prev_label = label;
                }
            }
        }

        TEST_F (SuRFBuilderUnitTest, buildSparseStringTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_bytes;
            for (int i=0; i<words.size(); i++) {
                words_bytes.emplace_back(std::make_pair(stringToByteVector(words[i].first),words[i].second));
            }
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_trunc_bytes;
            for (int i=0; i<words_trunc_.size(); i++) {
                words_trunc_bytes.emplace_back(std::make_pair(stringToByteVector(words_trunc_[i].first),words_trunc_[i].second));
            }
            bool include_dense = false;
            uint32_t sparse_dense_ratio = 0;
            level_t suffix_len_array[5] = {1, 3, 7, 8, 13};
            for (int i = 0; i < 5; i++) {
                level_t suffix_len = suffix_len_array[i];
                builder_ = new SuRFBuilder<uint64_t>(include_dense, sparse_dense_ratio, kReal, 0, suffix_len);
                builder_->build(words_bytes);
                testSparse(words_bytes, words_trunc_bytes);
                delete builder_;
            }
        }

        TEST_F (SuRFBuilderUnitTest, buildSparseDuplicateTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_bytes;
            for (int i=0; i<words.size(); i++) {
                words_bytes.emplace_back(std::make_pair(stringToByteVector(words[i].first),words[i].second));
            }
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_trunc_bytes;
            for (int i=0; i<words_trunc_.size(); i++) {
                words_trunc_bytes.emplace_back(std::make_pair(stringToByteVector(words_trunc_[i].first),words_trunc_[i].second));
            }
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_dup_bytes;
            for (int i=0; i<words_dup.size(); i++) {
                words_dup_bytes.emplace_back(std::make_pair(stringToByteVector(words_dup[i].first),words_dup[i].second));
            }
            bool include_dense = false;
            uint32_t sparse_dense_ratio = 0;
            level_t suffix_len_array[5] = {1, 3, 7, 8, 13};
            for (int i = 0; i < 5; i++) {
                level_t suffix_len = suffix_len_array[i];
                builder_ = new SuRFBuilder<uint64_t>(include_dense, sparse_dense_ratio, kReal, 0, suffix_len);
                builder_->build(words_dup_bytes);
                testSparse(words_bytes, words_trunc_bytes);
                delete builder_;
            }
        }

        TEST_F (SuRFBuilderUnitTest, buildSparseIntTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_bytes;
            for (int i=0; i<ints_.size(); i++) {
                ints_bytes.emplace_back(std::make_pair(stringToByteVector(ints_[i].first),ints_[i].second));
            }
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_trunc_bytes;
            for (int i=0; i<ints_trunc_.size(); i++) {
                ints_trunc_bytes.emplace_back(std::make_pair(stringToByteVector(ints_trunc_[i].first),ints_trunc_[i].second));
            }
            bool include_dense = false;
            uint32_t sparse_dense_ratio = 0;
            level_t suffix_len_array[5] = {1, 3, 7, 8, 13};
            for (int i = 0; i < 5; i++) {
                level_t suffix_len = suffix_len_array[i];
                builder_ = new SuRFBuilder<uint64_t>(include_dense, sparse_dense_ratio, kReal, 0, suffix_len);
                builder_->build(ints_bytes);
                testSparse(ints_bytes, ints_trunc_bytes);
                delete builder_;
            }
        }

        TEST_F (SuRFBuilderUnitTest, buildDenseStringTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> words_bytes;
            for (int i=0; i<words.size(); i++) {
                words_bytes.emplace_back(std::make_pair(stringToByteVector(words[i].first),words[i].second));
            }
            bool include_dense = true;
            uint32_t sparse_dense_ratio = 0;
            level_t suffix_len_array[5] = {1, 3, 7, 8, 13};
            for (int i = 0; i < 5; i++) {
                level_t suffix_len = suffix_len_array[i];
                builder_ = new SuRFBuilder<uint64_t>(include_dense, sparse_dense_ratio, kReal, 0, suffix_len);
                builder_->build(words_bytes);
                testDense();
                delete builder_;
            }
        }

        TEST_F (SuRFBuilderUnitTest, buildDenseIntTest) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> ints_bytes;
            for (int i=0; i<ints_.size(); i++) {
                ints_bytes.emplace_back(std::make_pair(stringToByteVector(ints_[i].first),ints_[i].second));
            }
            bool include_dense = true;
            uint32_t sparse_dense_ratio = 0;
            level_t suffix_len_array[5] = {1, 3, 7, 8, 13};
            for (int i = 0; i < 5; i++) {
                level_t suffix_len = suffix_len_array[i];
                builder_ = new SuRFBuilder<uint64_t>(include_dense, sparse_dense_ratio, kReal, 0, suffix_len);
                builder_->build(ints_bytes);
                testDense();
                delete builder_;
            }
        }

    } // namespace buildertest

} // namespace surf