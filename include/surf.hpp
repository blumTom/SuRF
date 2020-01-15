#ifndef SURF_H_
#define SURF_H_

#include <string>
#include <vector>
#include <optional>

#include "config.hpp"
#include "louds_dense.hpp"
#include "louds_sparse.hpp"
#include "surf_builder.hpp"

namespace surf {

    class SuRF {
    public:
        class Iter {
        public:
            Iter() {};

            Iter(const SuRF *filter) {
                dense_iter_ = LoudsDense::Iter(filter->louds_dense_);
                sparse_iter_ = LoudsSparse::Iter(filter->louds_sparse_);
                could_be_fp_ = false;
            }

            void clear();

            bool isValid() const;

            bool getFpFlag() const;

            int compare(const std::vector<label_t> &key) const;

            std::vector<label_t> getKey() const;

            int getSuffix(word_t *suffix) const;

            uint64_t getValue() const;

            std::vector<label_t> getKeyWithSuffix(unsigned *bitlen) const;

            // Returns true if the status of the iterator after the operation is valid
            bool operator++(int);

            bool operator--(int);

            std::vector<position_t> getPosition(bool isDense) {
                return isDense ? dense_iter_.getPositions() : sparse_iter_.getPosition();
            }

        private:
            void passToSparse();

            bool incrementDenseIter();

            bool incrementSparseIter();

            bool decrementDenseIter();

            bool decrementSparseIter();

        private:
            // true implies that dense_iter_ is valid
            LoudsDense::Iter dense_iter_;
            LoudsSparse::Iter sparse_iter_;
            bool could_be_fp_;

            friend class SuRF;
        };

    public:
        SuRF() {};

        //------------------------------------------------------------------
        // Input keys must be SORTED
        //------------------------------------------------------------------
        SuRF(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys) {
            create(keys, kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
        }

        SuRF(const std::vector<std::string> &keys) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const std::string &key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(stringToByteVector(key),counter));
            }

            create(keyStrs, kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
        }

        SuRF(const std::vector<uint32_t> &keys) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const uint32_t &key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(uint32ToByteVector(key),counter));
            }

            create(keyStrs, kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
        }

        SuRF(const std::vector<uint64_t> &keys) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const uint64_t key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(uint64ToByteVector(key),counter));
            }

            create(keyStrs, kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
        }

        SuRF(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            create(keys, kIncludeDense, kSparseDenseRatio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(const std::vector<std::string> &keys,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const std::string &key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(stringToByteVector(key),counter));
            }

            create(keyStrs, kIncludeDense, kSparseDenseRatio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(const std::vector<uint32_t> &keys,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const uint32_t key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(uint32ToByteVector(key),counter));
            }

            create(keyStrs, kIncludeDense, kSparseDenseRatio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(const std::vector<uint64_t> &keys,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const uint64_t key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(uint64ToByteVector(key),counter));
            }

            create(keyStrs, kIncludeDense, kSparseDenseRatio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys,
             const bool include_dense,
             const uint32_t sparse_dense_ratio,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            create(keys, include_dense, sparse_dense_ratio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(const std::vector<std::string> &keys,
             const bool include_dense,
             const uint32_t sparse_dense_ratio,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const std::string &key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(stringToByteVector(key),counter));
            }

            create(keyStrs, include_dense, sparse_dense_ratio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(const std::vector<uint32_t> &keys,
             const bool include_dense,
             const uint32_t sparse_dense_ratio,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const uint32_t key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(uint32ToByteVector(key),counter));
            }

            create(keyStrs, include_dense, sparse_dense_ratio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(const std::vector<uint64_t> &keys,
             const bool include_dense,
             const uint32_t sparse_dense_ratio,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len) {
            std::vector<std::pair<std::vector<label_t>,uint64_t>> keyStrs;

            uint64_t counter = 0;
            for (const uint64_t key : keys) {
                counter++;
                keyStrs.emplace_back(std::make_pair(uint64ToByteVector(key),counter));
            }

            create(keyStrs, include_dense, sparse_dense_ratio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        ~SuRF() {}

        void create(const std::vector<std::pair<std::vector<label_t>,uint64_t>> &keys,
                    const bool include_dense,
                    const uint32_t sparse_dense_ratio,
                    const SuffixType suffix_type,
                    const level_t hash_suffix_len,
                    const level_t real_suffix_len);

        std::optional<uint64_t> lookupKey(const std::vector<label_t> &key) const;

        std::optional<uint64_t> lookupKey(const std::string &key) const;

        std::optional<uint64_t> lookupKey(const uint32_t &key) const;

        std::optional<uint64_t> lookupKey(const uint64_t &key) const;

        // This function searches in a conservative way: if inclusive is true
        // and the stored key prefix matches key, iter stays at this key prefix.
        SuRF::Iter moveToKeyGreaterThan(const std::vector<label_t> &key, const bool inclusive) const;

        SuRF::Iter moveToKeyGreaterThan(const std::string &key, const bool inclusive) const;

        SuRF::Iter moveToKeyLessThan(const std::vector<label_t> &key, const bool inclusive) const;

        SuRF::Iter moveToKeyLessThan(const std::string &key, const bool inclusive) const;

        SuRF::Iter moveToFirst() const;

        SuRF::Iter moveToLast() const;

        std::vector<uint64_t> lookupRange(const std::vector<label_t> &left_key,
                         const bool left_inclusive,
                         const std::vector<label_t> &right_key,
                         const bool right_inclusive);

        std::vector<uint64_t> lookupRange(const std::string &left_key,
                         const bool left_inclusive,
                         const std::string &right_key,
                         const bool right_inclusive);

        std::vector<uint64_t> lookupRange(const uint32_t &left_key,
                         const bool left_inclusive,
                         const uint32_t &right_key,
                         const bool right_inclusive);

        std::vector<uint64_t> lookupRange(const uint64_t &left_key,
                         const bool left_inclusive,
                         const uint64_t &right_key,
                         const bool right_inclusive);

        uint64_t serializedSize() const;

        uint64_t getMemoryUsage() const;

        level_t getHeight() const;

        level_t getSparseStartLevel() const;

        char *serialize() const {
            uint64_t size = serializedSize();
            char *data = new char[size];
            char *cur_data = data;
            louds_dense_->serialize(cur_data);
            louds_sparse_->serialize(cur_data);
            assert(cur_data - data == (int64_t) size);
            return data;
        }

        static SuRF *deSerialize(char *src) {
            SuRF *surf = new SuRF();
            surf->louds_dense_ = LoudsDense::deSerialize(src);
            surf->louds_sparse_ = LoudsSparse::deSerialize(src);
            surf->iter_ = SuRF::Iter(surf);
            return surf;
        }

        void destroy() {
            louds_dense_->destroy();
            louds_sparse_->destroy();
        }

    private:
        LoudsDense *louds_dense_;
        LoudsSparse *louds_sparse_;
        SuRFBuilder *builder_;
        SuRF::Iter iter_;

        shared_ptr<std::vector<std::vector<uint64_t>>> values_;
    };

} // namespace surf

#endif // SURF_H
