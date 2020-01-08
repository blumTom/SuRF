#include "gtest/gtest.h"

#include <assert.h>

#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "surf.hpp"

namespace surf {

    namespace surftest {

        static const std::string kFilePath = "../../../test/words.txt";
        static const int kWordTestSize = 1000;
        static const int kIntTestBound = 1001;
        static const uint64_t kIntTestSkip = 10;
        static const int kNumSuffixType = 4;
        static const SuffixType kSuffixTypeList[kNumSuffixType] = {kNone, kHash, kReal, kMixed};
        static const int kNumSuffixLen = 6;
        static const level_t kSuffixLenList[kNumSuffixLen] = {1, 3, 7, 8, 13, 26};
        static std::vector<std::pair<std::vector<label_t>,uint64_t>> words;

        class SuRFUnitTest : public ::testing::Test {
        public:
            virtual void SetUp() {
                truncateWordSuffixes();
                fillinInts();
                data_ = nullptr;
            }

            virtual void TearDown() {
                if (data_)
                    delete[] data_;
            }

            void newSuRFWords(SuffixType suffix_type, level_t suffix_len);

            void newSuRFInts(SuffixType suffix_type, level_t suffix_len);

            void truncateWordSuffixes();

            void fillinInts();

            void testSerialize();

            void testLookupWord(SuffixType suffix_type);

            SuRF *surf_;
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

        static bool isEqual(const std::vector<label_t> &a, const std::vector<label_t> &b, const unsigned keylen, const unsigned bitlen) {
            if (bitlen == 0) {
                return isSameKey(a,b,keylen);
            } else {
                if (!isSameKey(a,b,keylen - 1)) return false;
                char mask = 0xFF << (8 - bitlen);
                char a_suf = a[keylen - 1] & mask;
                char b_suf = b[keylen - 1] & mask;
                return (a_suf == b_suf);
            }
        }

        void SuRFUnitTest::newSuRFWords(SuffixType suffix_type, level_t suffix_len) {
            if (suffix_type == kNone)
                surf_ = new SuRF(words);
            else if (suffix_type == kHash)
                surf_ = new SuRF(words, kHash, suffix_len, 0);
            else if (suffix_type == kReal)
                surf_ = new SuRF(words, kIncludeDense, kSparseDenseRatio, kReal, 0, suffix_len);
            else if (suffix_type == kMixed)
                surf_ = new SuRF(words, kMixed, suffix_len, suffix_len);
            else
                surf_ = new SuRF(words);
        }

        void SuRFUnitTest::newSuRFInts(SuffixType suffix_type, level_t suffix_len) {
            if (suffix_type == kNone)
                surf_ = new SuRF(ints_);
            else if (suffix_type == kHash)
                surf_ = new SuRF(ints_, kHash, suffix_len, 0);
            else if (suffix_type == kReal)
                surf_ = new SuRF(ints_, kIncludeDense, kSparseDenseRatio, kReal, 0, suffix_len);
            else if (suffix_type == kMixed)
                surf_ = new SuRF(ints_, kMixed, suffix_len, suffix_len);
            else
                surf_ = new SuRF(ints_);
        }

        void SuRFUnitTest::truncateWordSuffixes() {
            assert(words.size() > 1);
            int commonPrefixLen = 0;
            for (unsigned i = 0; i < words.size(); i++) {
                if (i == 0)
                    commonPrefixLen = getCommonPrefixLen(words[i].first, words[i + 1].first);
                else if (i == words.size() - 1)
                    commonPrefixLen = getCommonPrefixLen(words[i - 1].first, words[i].first);
                else
                    commonPrefixLen = getMax(getCommonPrefixLen(words[i - 1].first, words[i].first),
                                             getCommonPrefixLen(words[i].first, words[i + 1].first));

                if (commonPrefixLen < (int) words[i].first.size()) {
                    std::vector<label_t> subVector;
                    for (int j=0; j<commonPrefixLen + 1; j++) {
                        subVector.emplace_back(words[i].first[j]);
                    }
                    words_trunc_.push_back({subVector,words[i].second});
                } else {
                    words_trunc_.push_back(words[i]);
                    words_trunc_[i].first.emplace_back(kTerminator);
                }
            }
        }

        void SuRFUnitTest::fillinInts() {
            for (uint64_t i = 0; i < kIntTestBound; i += kIntTestSkip) {
                ints_.push_back({uint64ToByteVector(i),i});
            }
        }

        void SuRFUnitTest::testSerialize() {
            data_ = surf_->serialize();
            surf_->destroy();
            delete surf_;
            char *data = data_;
            surf_ = SuRF::deSerialize(data);
        }

        void SuRFUnitTest::testLookupWord(SuffixType suffix_type) {
            for (unsigned i = 0; i < words.size(); i++) {
                bool key_exist = surf_->lookupKey(words[i].first).has_value();
                ASSERT_TRUE(key_exist);
            }

            if (suffix_type == kNone) return;

            for (unsigned i = 0; i < words.size(); i++) {
                for (unsigned j = 0; j < words_trunc_[i].first.size() && j < words[i].first.size(); j++) {
                    std::vector<label_t> key = words[i].first;
                    key[j] = 'A';
                    bool key_exist = surf_->lookupKey(key).has_value();
                    ASSERT_FALSE(key_exist);
                }
            }
        }

        TEST_F (SuRFUnitTest, IntStringConvertTest) {
            for (uint64_t i = 0; i < kIntTestBound; i++) {
                ASSERT_EQ(i, stringToUint64(uint64ToString(i)));
            }
        }

        TEST_F (SuRFUnitTest, lookupWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFWords(kSuffixTypeList[t], kSuffixLenList[k]);
                    testLookupWord(kSuffixTypeList[t]);
                    surf_->destroy();
                    delete surf_;
                }
            }
        }

        /*TEST_F (SuRFUnitTest, serializeTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFWords(kSuffixTypeList[t], kSuffixLenList[k]);
                    testSerialize();
                    testLookupWord(kSuffixTypeList[t]);
                }
            }
        }*/

        TEST_F (SuRFUnitTest, lookupIntTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFInts(kSuffixTypeList[t], kSuffixLenList[k]);
                    for (uint64_t i = 0; i < kIntTestBound; i += kIntTestSkip) {
                        bool key_exist = surf_->lookupKey(uint64ToString(i)).has_value();;
                        if (i % kIntTestSkip == 0)
                            ASSERT_TRUE(key_exist);
                        else
                            ASSERT_FALSE(key_exist);
                    }
                    surf_->destroy();
                    delete surf_;
                }
            }
        }

        TEST_F (SuRFUnitTest, moveToKeyGreaterThanWordTest) {
            for (int t = 2; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFWords(kSuffixTypeList[t], kSuffixLenList[k]);
                    bool inclusive = true;
                    for (int i = 0; i < 2; i++) {
                        if (i == 1)
                            inclusive = false;
                        for (int j = -1; j <= (int) words.size(); j++) {
                            SuRF::Iter iter;
                            if (j < 0)
                                iter = surf_->moveToFirst();
                            else if (j >= (int) words.size())
                                iter = surf_->moveToLast();
                            else
                                iter = surf_->moveToKeyGreaterThan(words[j].first, inclusive);

                            unsigned bitlen;
                            bool is_prefix;
                            if (j < 0) {
                                ASSERT_TRUE(iter.isValid());
                                std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                                is_prefix = isEqual(stringToByteVector(iter_key), words[0].first, iter_key.length(), bitlen);
                                ASSERT_TRUE(is_prefix);
                            } else if (j >= (int) words.size()) {
                                ASSERT_TRUE(iter.isValid());
                                std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                                is_prefix = isEqual(stringToByteVector(iter_key), words[words.size() - 1].first, iter_key.length(), bitlen);
                                ASSERT_TRUE(is_prefix);
                            } else if (j == (int) words.size() - 1) {
                                if (iter.getFpFlag()) {
                                    ASSERT_TRUE(iter.isValid());
                                    std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                                    is_prefix = isEqual(stringToByteVector(iter_key), words[words.size() - 1].first, iter_key.length(), bitlen);
                                    ASSERT_TRUE(is_prefix);
                                } else {
                                    ASSERT_FALSE(iter.isValid());
                                }
                            } else {
                                ASSERT_TRUE(iter.isValid());
                                std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                                if (iter.getFpFlag())
                                    is_prefix = isEqual(stringToByteVector(iter_key), words[j].first, iter_key.length(), bitlen);
                                else
                                    is_prefix = isEqual(stringToByteVector(iter_key), words[j + 1].first, iter_key.length(), bitlen);
                                ASSERT_TRUE(is_prefix);

                                // test getKey()
                                std::string iter_get_key = iter.getKey();
                                std::string iter_key_prefix = iter_key.substr(0, iter_get_key.length());
                                is_prefix = (iter_key_prefix.compare(iter_get_key) == 0);
                                ASSERT_TRUE(is_prefix);

                                // test getSuffix()
                                if (kSuffixTypeList[t] == kReal || kSuffixTypeList[t] == kMixed) {
                                    word_t iter_suffix = 0;
                                    int iter_suffix_len = iter.getSuffix(&iter_suffix);
                                    ASSERT_EQ(kSuffixLenList[k], iter_suffix_len);
                                    std::string iter_key_suffix_str
                                            = iter_key.substr(iter_get_key.length(), iter_key.length());
                                    word_t iter_key_suffix = 0;
                                    int suffix_len = (int) kSuffixLenList[k];
                                    int suffix_str_len = (int) (iter_key.length() - iter_get_key.length());
                                    level_t pos = 0;
                                    while (suffix_len > 0 && suffix_str_len > 0) {
                                        iter_key_suffix += (word_t) iter_key_suffix_str[pos];
                                        iter_key_suffix <<= 8;
                                        suffix_len -= 8;
                                        suffix_str_len--;
                                        pos++;
                                    }
                                    if (pos > 0) {
                                        iter_key_suffix >>= 8;
                                        if (kSuffixLenList[k] % 8 != 0)
                                            iter_key_suffix >>= (8 - (kSuffixLenList[k] % 8));
                                    }
                                    ASSERT_EQ(iter_key_suffix, iter_suffix);
                                }
                            }
                        }
                    }
                    surf_->destroy();
                    delete surf_;
                }
            }
        }


        TEST_F (SuRFUnitTest, moveToKeyGreaterThanIntTest) {
            for (int k = 0; k < kNumSuffixLen; k++) {
                newSuRFInts(kMixed, kSuffixLenList[k]);
                bool inclusive = true;
                for (int i = 0; i < 2; i++) {
                    if (i == 1)
                        inclusive = false;
                    for (uint64_t j = 0; j < kIntTestBound - 1; j++) {
                        SuRF::Iter iter = surf_->moveToKeyGreaterThan(uint64ToString(j), inclusive);

                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix;
                        if (iter.getFpFlag())
                            is_prefix = isEqual(stringToByteVector(iter_key),stringToByteVector(uint64ToString(j - (j % kIntTestSkip))), iter_key.length(), bitlen);
                        else
                            is_prefix = isEqual(stringToByteVector(iter_key),stringToByteVector(uint64ToString(j - (j % kIntTestSkip) + kIntTestSkip)), iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    }

                    SuRF::Iter iter = surf_->moveToKeyGreaterThan(uint64ToString(kIntTestBound - 1), inclusive);
                    if (iter.getFpFlag()) {
                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key),stringToByteVector(uint64ToString(kIntTestBound - 1)), iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    } else {
                        ASSERT_FALSE(iter.isValid());
                    }
                }
                surf_->destroy();
                delete surf_;
            }
        }

        TEST_F (SuRFUnitTest, moveToKeyLessThanWordTest) {
            for (int k = 0; k < kNumSuffixLen; k++) {
                newSuRFWords(kMixed, kSuffixLenList[k]);
                bool inclusive = true;
                for (int i = 0; i < 2; i++) {
                    if (i == 1)
                        inclusive = false;
                    for (unsigned j = 1; j < words.size(); j++) {
                        SuRF::Iter iter = surf_->moveToKeyLessThan(words[j].first, inclusive);

                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = false;
                        if (iter.getFpFlag())
                            is_prefix = isEqual(stringToByteVector(iter_key), words[j].first, iter_key.length(), bitlen);
                        else
                            is_prefix = isEqual(stringToByteVector(iter_key), words[j - 1].first, iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    }

                    SuRF::Iter iter = surf_->moveToKeyLessThan(words[0].first, inclusive);
                    if (iter.getFpFlag()) {
                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key), words[0].first, iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    } else {
                        ASSERT_FALSE(iter.isValid());
                    }
                }
                surf_->destroy();
                delete surf_;
            }
        }

        TEST_F (SuRFUnitTest, moveToKeyLessThanIntTest) {
            for (int k = 0; k < kNumSuffixLen; k++) {
                newSuRFInts(kMixed, kSuffixLenList[k]);
                bool inclusive = true;
                for (int i = 0; i < 2; i++) {
                    if (i == 1)
                        inclusive = false;
                    for (uint64_t j = kIntTestSkip; j < kIntTestBound; j++) {
                        SuRF::Iter iter = surf_->moveToKeyLessThan(uint64ToString(j), inclusive);

                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key),stringToByteVector(uint64ToString(j - (j % kIntTestSkip))), iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    }
                    SuRF::Iter iter = surf_->moveToKeyLessThan(uint64ToString(0), inclusive);
                    if (iter.getFpFlag()) {
                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key),stringToByteVector(uint64ToString(0)), iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    } else {
                        ASSERT_FALSE(iter.isValid());
                    }
                }
                surf_->destroy();
                delete surf_;
            }
        }


        TEST_F (SuRFUnitTest, IteratorIncrementWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFWords(kSuffixTypeList[t], kSuffixLenList[k]);
                    bool inclusive = true;
                    SuRF::Iter iter = surf_->moveToKeyGreaterThan(words[0].first, inclusive);
                    for (unsigned i = 1; i < words.size(); i++) {
                        iter++;
                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key), words[i].first, iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    }
                    iter++;
                    ASSERT_FALSE(iter.isValid());
                    surf_->destroy();
                    delete surf_;
                }
            }
        }

        TEST_F (SuRFUnitTest, IteratorIncrementIntTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFInts(kSuffixTypeList[t], kSuffixLenList[k]);
                    bool inclusive = true;
                    SuRF::Iter iter = surf_->moveToKeyGreaterThan(uint64ToString(0), inclusive);
                    for (uint64_t i = kIntTestSkip; i < kIntTestBound; i += kIntTestSkip) {
                        iter++;
                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key),stringToByteVector(uint64ToString(i)), iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    }
                    iter++;
                    ASSERT_FALSE(iter.isValid());
                    surf_->destroy();
                    delete surf_;
                }
            }
        }

        TEST_F (SuRFUnitTest, IteratorDecrementWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFWords(kSuffixTypeList[t], kSuffixLenList[k]);
                    bool inclusive = true;
                    SuRF::Iter iter = surf_->moveToKeyGreaterThan(words[words.size() - 1].first, inclusive);
                    for (int i = words.size() - 2; i >= 0; i--) {
                        iter--;
                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key), words[i].first, iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    }
                    iter--;
                    ASSERT_FALSE(iter.isValid());
                    surf_->destroy();
                    delete surf_;
                }
            }
        }

        TEST_F (SuRFUnitTest, IteratorDecrementIntTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFInts(kSuffixTypeList[t], kSuffixLenList[k]);
                    bool inclusive = true;
                    SuRF::Iter iter = surf_->moveToKeyGreaterThan(uint64ToString(kIntTestBound - kIntTestSkip),
                                                                  inclusive);
                    for (uint64_t i = kIntTestBound - 1 - kIntTestSkip; i > 0; i -= kIntTestSkip) {
                        iter--;
                        ASSERT_TRUE(iter.isValid());
                        unsigned bitlen;
                        std::string iter_key = iter.getKeyWithSuffix(&bitlen);
                        bool is_prefix = isEqual(stringToByteVector(iter_key),stringToByteVector(uint64ToString(i)), iter_key.length(), bitlen);
                        ASSERT_TRUE(is_prefix);
                    }
                    iter--;
                    iter--;
                    ASSERT_FALSE(iter.isValid());
                    surf_->destroy();
                    delete surf_;
                }
            }
        }

        TEST_F (SuRFUnitTest, lookupRangeWordTest) {
            for (int t = 0; t < kNumSuffixType; t++) {
                for (int k = 0; k < kNumSuffixLen; k++) {
                    newSuRFWords(kSuffixTypeList[t], kSuffixLenList[k]);
                    bool exist = surf_->lookupRange(stringToByteVector(std::string("\1")), true, words[0].first, true).size() > 0;
                    ASSERT_TRUE(exist);
                    exist = surf_->lookupRange(stringToByteVector(std::string("\1")), true, words[0].first, false).size() > 0;
                    ASSERT_TRUE(exist);
                    for (unsigned i = 0; i < words.size() - 1; i++) {
                        exist = surf_->lookupRange(words[i].first, true, words[i + 1].first, true).size() > 0;
                        ASSERT_TRUE(exist);
                        exist = surf_->lookupRange(words[i].first, true, words[i + 1].first, false).size() > 0;
                        ASSERT_TRUE(exist);
                        exist = surf_->lookupRange(words[i].first, false, words[i + 1].first, true).size() > 0;
                        ASSERT_TRUE(exist);
                        exist = surf_->lookupRange(words[i].first, false, words[i + 1].first, false).size() > 0;
                        ASSERT_TRUE(exist);
                    }
                    exist = surf_->lookupRange(words[words.size() - 1].first, true, stringToByteVector(std::string("zzzzzzzz")), false).size() > 0;
                    ASSERT_TRUE(exist);
                    exist = surf_->lookupRange(words[words.size() - 1].first, false, stringToByteVector(std::string("zzzzzzzz")), false).size() > 0;
                    ASSERT_TRUE(exist);
                }
            }
        }

        TEST_F (SuRFUnitTest, lookupRangeIntTest) {
            for (int k = 0; k < kNumSuffixLen; k++) {
                newSuRFInts(kMixed, kSuffixLenList[k]);
                for (uint64_t i = 0; i < kIntTestBound; i++) {
                    bool exist = surf_->lookupRange(uint64ToString(i), true,
                                                    uint64ToString(i), true).size() > 0;
                    if (i % kIntTestSkip == 0)
                        ASSERT_TRUE(exist);
                    else
                        ASSERT_FALSE(exist);

                    for (unsigned j = 1; j < kIntTestSkip + 2; j++) {
                        exist = surf_->lookupRange(uint64ToString(i), false,
                                                   uint64ToString(i + j), true).size() > 0;
                        uint64_t left_bound_interval_id = i / kIntTestSkip;
                        uint64_t right_bound_interval_id = (i + j) / kIntTestSkip;
                        if ((i % kIntTestSkip == 0)
                            || ((i < kIntTestBound - 1)
                                && ((left_bound_interval_id < right_bound_interval_id)
                                    || ((i + j) % kIntTestSkip == 0))))
                            ASSERT_TRUE(exist);
                        else
                            ASSERT_FALSE(exist);
                    }
                }
            }
        }

        void loadWordList() {
            std::ifstream infile(kFilePath);
            std::string keyStr;
            int count = 0;
            while (infile.good() && count < kWordTestSize) {
                infile >> keyStr;
                std::vector<label_t> key;
                for (int i=0; i<keyStr.length(); i++) {
                    key.emplace_back(keyStr[i]);
                }
                words.push_back({key,count});
                count++;
            }
        }

    } // namespace surftest

} // namespace surf

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    surf::surftest::loadWordList();
    return RUN_ALL_TESTS();
}
