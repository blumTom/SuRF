#include "surf.hpp"

namespace surf {

    void SuRF::create(const std::vector<std::pair<std::vector<label_t>, uint64_t>> &keys,
                      const bool include_dense,
                      const uint32_t sparse_dense_ratio,
                      const SuffixType suffix_type,
                      const level_t hash_suffix_len,
                      const level_t real_suffix_len) {
        builder_ = new SuRFBuilder(include_dense, sparse_dense_ratio, suffix_type, hash_suffix_len, real_suffix_len);
        builder_->build(keys);
        louds_dense_ = new LoudsDense(builder_);
        louds_sparse_ = new LoudsSparse(builder_);
        iter_ = SuRF::Iter(this);
        delete builder_;
    }

    std::optional<uint64_t> SuRF::lookupKey(const std::vector<label_t> &key) const {
        position_t connect_node_num = 0;
        std::optional<uint64_t> result = std::nullopt;


        result = louds_dense_->lookupKey(key, connect_node_num);
        if (result.has_value() && connect_node_num != 0) {
            result = louds_sparse_->lookupKey(key, connect_node_num);
        }

        return result;
    }

    std::optional<uint64_t> SuRF::lookupKey(const std::string &key) const {
        return lookupKey(stringToByteVector(key));
    }

    std::optional<uint64_t> SuRF::lookupKey(const uint32_t &key) const {
        return lookupKey(uint32ToByteVector(key));
    }

    std::optional<uint64_t> SuRF::lookupKey(const uint64_t &key) const {
        return lookupKey(uint64ToByteVector(key));
    }

    SuRF::Iter SuRF::moveToKeyGreaterThan(const std::vector<label_t> &key, const bool inclusive) const {
        SuRF::Iter iter(this);
        iter.could_be_fp_ = louds_dense_->moveToKeyGreaterThan(key, inclusive, iter.dense_iter_);

        if (!iter.dense_iter_.isValid()) {
            return iter;
        }
        if (iter.dense_iter_.isComplete()) {
            return iter;
        }

        if (!iter.dense_iter_.isSearchComplete()) {
            iter.passToSparse();
            iter.could_be_fp_ = louds_sparse_->moveToKeyGreaterThan(key, inclusive, iter.sparse_iter_);

            if (!iter.sparse_iter_.isValid()) iter.incrementDenseIter();

            return iter;
        } else if (!iter.dense_iter_.isMoveLeftComplete()) {
            iter.passToSparse();
            iter.sparse_iter_.moveToLeftMostKey();
            return iter;
        }

        assert(false); // shouldn't reach here
        return iter;
    }

    SuRF::Iter SuRF::moveToKeyGreaterThan(const std::string &key, const bool inclusive) const {
        return moveToKeyGreaterThan(stringToByteVector(key), inclusive);
    }

    SuRF::Iter SuRF::moveToKeyLessThan(const std::vector<label_t> &key, const bool inclusive) const {
        SuRF::Iter iter = moveToKeyGreaterThan(key, false);
        if (!iter.isValid()) {
            iter = moveToLast();
            return iter;
        }
        if (!iter.getFpFlag()) {
            iter--;
            if (lookupKey(key))
                iter--;
        }
        return iter;
    }

    SuRF::Iter SuRF::moveToKeyLessThan(const std::string &key, const bool inclusive) const {
        return moveToKeyLessThan(stringToByteVector(key), inclusive);
    }

    SuRF::Iter SuRF::moveToFirst() const {
        SuRF::Iter iter(this);
        if (louds_dense_->getHeight() > 0) {
            iter.dense_iter_.setToFirstLabelInRoot();
            iter.dense_iter_.moveToLeftMostKey();

            if (iter.dense_iter_.isMoveLeftComplete()) return iter;
            iter.passToSparse();
            iter.sparse_iter_.moveToLeftMostKey();
        } else {
            iter.sparse_iter_.setToFirstLabelInRoot();
            iter.sparse_iter_.moveToLeftMostKey();
        }
        return iter;
    }

    SuRF::Iter SuRF::moveToLast() const {
        SuRF::Iter iter(this);
        if (louds_dense_->getHeight() > 0) {
            iter.dense_iter_.setToLastLabelInRoot();
            iter.dense_iter_.moveToRightMostKey();

            if (iter.dense_iter_.isMoveRightComplete()) return iter;

            iter.passToSparse();
            iter.sparse_iter_.moveToRightMostKey();
        } else {
            iter.sparse_iter_.setToLastLabelInRoot();
            iter.sparse_iter_.moveToRightMostKey();
        }
        return iter;
    }

    std::vector<uint64_t> SuRF::lookupRange(const std::vector<label_t> &left_key, const bool left_inclusive,
                                            const std::vector<label_t> &right_key, const bool right_inclusive) {

        iter_.clear();
        louds_dense_->moveToKeyGreaterThan(left_key, left_inclusive, iter_.dense_iter_);
        if (!iter_.dense_iter_.isValid()) return std::vector<uint64_t>(0);
        if (!iter_.dense_iter_.isComplete()) {
            if (!iter_.dense_iter_.isSearchComplete()) {
                iter_.passToSparse();
                louds_sparse_->moveToKeyGreaterThan(left_key, left_inclusive, iter_.sparse_iter_);
                if (!iter_.sparse_iter_.isValid()) {
                    iter_.incrementDenseIter();
                }
            } else if (!iter_.dense_iter_.isMoveLeftComplete()) {
                iter_.passToSparse();
                iter_.sparse_iter_.moveToLeftMostKey();
            }
        }


        std::vector<uint64_t> resultSet = std::vector<uint64_t>(0);
        for (; iter_.isValid(); iter_++) {
            int compare = iter_.compare(right_key);

            if (compare == kCouldBePositive ||
                (right_inclusive && (compare <= 0)) ||
                (!right_inclusive && (compare < 0))) {
                uint64_t value = iter_.getValue();
                resultSet.emplace_back(value);
            }
        }

        return resultSet;
    }

    std::vector<uint64_t> SuRF::lookupRange(const std::string &left_key, const bool left_inclusive,
                                            const std::string &right_key, const bool right_inclusive) {
        return lookupRange(stringToByteVector(left_key), left_inclusive, stringToByteVector(right_key),
                           right_inclusive);
    }

    std::vector<uint64_t> SuRF::lookupRange(const uint32_t &left_key,
                                            const bool left_inclusive,
                                            const uint32_t &right_key,
                                            const bool right_inclusive) {
        return lookupRange(uint32ToByteVector(left_key), left_inclusive, uint32ToByteVector(right_key),
                           right_inclusive);
    }

    std::vector<uint64_t> SuRF::lookupRange(const uint64_t &left_key,
                                            const bool left_inclusive,
                                            const uint64_t &right_key,
                                            const bool right_inclusive) {
        return lookupRange(uint64ToByteVector(left_key), left_inclusive, uint64ToByteVector(right_key),
                           right_inclusive);
    }

    uint64_t SuRF::serializedSize() const {
        return (louds_dense_->serializedSize()
                + louds_sparse_->serializedSize());
    }

    uint64_t SuRF::getMemoryUsage() const {
        return (sizeof(SuRF) + louds_dense_->getMemoryUsage() + louds_sparse_->getMemoryUsage());
    }

    level_t SuRF::getHeight() const {
        return louds_sparse_->getHeight();
    }

    level_t SuRF::getSparseStartLevel() const {
        return louds_sparse_->getStartLevel();
    }

//============================================================================

    void SuRF::Iter::clear() {
        dense_iter_.clear();
        sparse_iter_.clear();
    }

    bool SuRF::Iter::getFpFlag() const {
        return could_be_fp_;
    }

    bool SuRF::Iter::isValid() const {
        return dense_iter_.isValid()
               && (dense_iter_.isComplete() || sparse_iter_.isValid());
    }

    int SuRF::Iter::compare(const std::vector<label_t> &key) const {
        assert(isValid());
        int dense_compare = dense_iter_.compare(key);
        if (dense_iter_.isComplete() || dense_compare != 0)
            return dense_compare;
        return sparse_iter_.compare(key);
    }

    std::vector<label_t> SuRF::Iter::getKey() const {
        if (!isValid())
            return std::vector<label_t>(0);
        if (dense_iter_.isComplete())
            return dense_iter_.getKey();
        std::vector<label_t> result = dense_iter_.getKey();
        std::vector<label_t> sparseKey = sparse_iter_.getKey();
        for (int i = 0; i < sparseKey.size(); i++) {
            result.emplace_back(sparseKey[i]);
        }
        return result;
    }

    int SuRF::Iter::getSuffix(word_t *suffix) const {
        if (!isValid())
            return 0;
        if (dense_iter_.isComplete())
            return dense_iter_.getSuffix(suffix);
        return sparse_iter_.getSuffix(suffix);
    }

    uint64_t SuRF::Iter::getValue() const {
        if (!isValid())
            return 0;
        if (dense_iter_.isComplete())
            return dense_iter_.getValue();
        return sparse_iter_.getValue();
    }

    std::vector<label_t> SuRF::Iter::getKeyWithSuffix(unsigned *bitlen) const {
        *bitlen = 0;
        if (!isValid())
            return stringToByteVector(std::string());
        if (dense_iter_.isComplete())
            return dense_iter_.getKeyWithSuffix(bitlen);

        std::vector<label_t> result = dense_iter_.getKeyWithSuffix(bitlen);
        std::vector<label_t> sparseKey = sparse_iter_.getKeyWithSuffix(bitlen);
        for (int i = 0; i < sparseKey.size(); i++) {
            result.emplace_back(sparseKey[i]);
        }
        return result;
    }

    void SuRF::Iter::passToSparse() {
        sparse_iter_.setStartNodeNum(dense_iter_.getSendOutNodeNum());
    }

    bool SuRF::Iter::incrementDenseIter() {
        if (!dense_iter_.isValid())
            return false;

        dense_iter_++;
        if (!dense_iter_.isValid())
            return false;
        if (dense_iter_.isMoveLeftComplete())
            return true;

        passToSparse();
        sparse_iter_.moveToLeftMostKey();
        return true;
    }

    bool SuRF::Iter::incrementSparseIter() {
        if (!sparse_iter_.isValid())
            return false;
        sparse_iter_++;
        return sparse_iter_.isValid();
    }

    bool SuRF::Iter::operator++(int) {
        if (!isValid())
            return false;
        if (incrementSparseIter())
            return true;
        return incrementDenseIter();
    }

    bool SuRF::Iter::decrementDenseIter() {
        if (!dense_iter_.isValid())
            return false;

        dense_iter_--;
        if (!dense_iter_.isValid())
            return false;
        if (dense_iter_.isMoveRightComplete())
            return true;

        passToSparse();
        sparse_iter_.moveToRightMostKey();
        return true;
    }

    bool SuRF::Iter::decrementSparseIter() {
        if (!sparse_iter_.isValid())
            return false;
        sparse_iter_--;
        return sparse_iter_.isValid();
    }

    bool SuRF::Iter::operator--(int) {
        if (!isValid())
            return false;
        if (decrementSparseIter())
            return true;
        return decrementDenseIter();
    }

}