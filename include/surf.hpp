#ifndef SURF_H_
#define SURF_H_

#include <vector>
#include <optional>

#include "config.hpp"
#include "louds_dense.hpp"
#include "louds_sparse.hpp"
#include "surf_builder.hpp"

namespace surf {

    template <typename Key, typename Value>
    class SuRF {
    public:
        class Iter {
        public:
            Iter() {};

            Iter(const SuRF *filter) {
                dense_iter_ = typename LoudsDense<Value>::Iter(filter->louds_dense_);
                sparse_iter_ = typename LoudsSparse<Value>::Iter(filter->louds_sparse_);
                could_be_fp_ = false;
            }

            void clear() {
                dense_iter_.clear();
                sparse_iter_.clear();
            }

            bool isValid() const {
                return dense_iter_.isValid()
                       && (dense_iter_.isComplete() || sparse_iter_.isValid());
            }

            bool getFpFlag() const {
                return could_be_fp_;
            }

            int compare(const std::vector<label_t> &key) const {
                assert(isValid());
                int dense_compare = dense_iter_.compare(key);
                if (dense_iter_.isComplete() || dense_compare != 0)
                    return dense_compare;
                return sparse_iter_.compare(key);
            }

            std::vector<label_t> getKey() const {
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

            int getSuffix(word_t *suffix) const {
                if (!isValid())
                    return 0;
                if (dense_iter_.isComplete())
                    return dense_iter_.getSuffix(suffix);
                return sparse_iter_.getSuffix(suffix);
            }

            Value getValue() const {
                if (!isValid())
                    return 0;
                if (dense_iter_.isComplete())
                    return dense_iter_.getValue();
                return sparse_iter_.getValue();
            }

            std::vector<label_t> getKeyWithSuffix(unsigned *bitlen) const {
                *bitlen = 0;
                if (!isValid())
                    return std::vector<label_t>(0);
                if (dense_iter_.isComplete())
                    return dense_iter_.getKeyWithSuffix(bitlen);

                std::vector<label_t> result = dense_iter_.getKeyWithSuffix(bitlen);
                std::vector<label_t> sparseKey = sparse_iter_.getKeyWithSuffix(bitlen);
                for (int i = 0; i < sparseKey.size(); i++) {
                    result.emplace_back(sparseKey[i]);
                }
                return result;
            }

            // Returns true if the status of the iterator after the operation is valid
            bool operator++(int) {
                if (!isValid())
                    return false;
                if (incrementSparseIter())
                    return true;
                return incrementDenseIter();
            }

            bool operator--(int) {
                if (!isValid())
                    return false;
                if (decrementSparseIter())
                    return true;
                return decrementDenseIter();
            }

            std::vector<position_t> getPosition(const bool isDense) const {
                return isDense ? dense_iter_.getPositions() : sparse_iter_.getPosition();
            }

        private:
            void passToSparse() {
                sparse_iter_.setStartNodeNum(dense_iter_.getSendOutNodeNum());
            }

            bool incrementDenseIter() {
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

            bool incrementSparseIter() {
                if (!sparse_iter_.isValid())
                    return false;
                sparse_iter_++;
                return sparse_iter_.isValid();
            }

            bool decrementDenseIter(){
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

            bool decrementSparseIter() {
                if (!sparse_iter_.isValid())
                    return false;
                sparse_iter_--;
                return sparse_iter_.isValid();
            }

        private:
            // true implies that dense_iter_ is valid
            typename LoudsDense<Value>::Iter dense_iter_;
            typename LoudsSparse<Value>::Iter sparse_iter_;
            bool could_be_fp_;

            friend class SuRF;
        };

    public:
        SuRF() {};

        //------------------------------------------------------------------
        // Input keys must be SORTED
        //------------------------------------------------------------------

        SuRF(std::vector<std::pair<Key,Value>> &keys, const std::function<std::vector<label_t>(const Key&)> keyDerivator) {
            keyDerivator_ = keyDerivator;
            std::vector<std::pair<std::vector<label_t>,Value>> keyBytes;

            for (int i=0; i<keys.size(); i++) {
                keyBytes.emplace_back(std::make_pair(keyDerivator(keys[i].first),keys[i].second));
            }

            create(keyBytes, kIncludeDense, kSparseDenseRatio, kNone, 0, 0);
        }

        SuRF(std::vector<std::pair<Key,Value>> &keys,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len,
             const std::function<std::vector<label_t>(const Key&)> keyDerivator) {
            keyDerivator_ = keyDerivator;
            std::vector<std::pair<std::vector<label_t>,Value>> keyBytes;

            for (int i=0; i<keys.size(); i++) {
                keyBytes.emplace_back(std::make_pair(keyDerivator(keys[i].first),keys[i].second));
            }

            create(keyBytes, kIncludeDense, kSparseDenseRatio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        SuRF(std::vector<std::pair<Key,Value>> &keys,
             const bool include_dense,
             const uint32_t sparse_dense_ratio,
             const SuffixType suffix_type,
             const level_t hash_suffix_len,
             const level_t real_suffix_len,
             const std::function<std::vector<label_t>(const Key&)> keyDerivator) {
            keyDerivator_ = keyDerivator;
            std::vector<std::pair<std::vector<label_t>,Value>> keyBytes;

            for (int i=0; i<keys.size(); i++) {
                keyBytes.emplace_back(std::make_pair(keyDerivator(keys[i].first),keys[i].second));
            }

            create(keyBytes, include_dense, sparse_dense_ratio, suffix_type, hash_suffix_len, real_suffix_len);
        }

        ~SuRF() {}

        void create(const std::vector<std::pair<std::vector<label_t>,Value>> &keys,
                    const bool include_dense,
                    const uint32_t sparse_dense_ratio,
                    const SuffixType suffix_type,
                    const level_t hash_suffix_len,
                    const level_t real_suffix_len) {
            builder_ = new SuRFBuilder<Value>(include_dense, sparse_dense_ratio, suffix_type, hash_suffix_len, real_suffix_len);
            builder_->build(keys);
            values_ = builder_->getValues();
            louds_dense_ = new LoudsDense(builder_);
            louds_sparse_ = new LoudsSparse(builder_);
            iter_ = SuRF::Iter(this);
            delete builder_;
        }

        std::optional<Value> lookupKey(const Key &key) const {
            return lookupKey(keyDerivator_(key));
        }

        // This function searches in a conservative way: if inclusive is true
        // and the stored key prefix matches key, iter stays at this key prefix.
        SuRF::Iter moveToKeyGreaterThan(const std::vector<label_t> &key, const bool inclusive) const {
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

        SuRF::Iter moveToKeyGreaterThan(const Key &key, const bool inclusive) const {
            return moveToKeyGreaterThan(keyDerivator_(key), inclusive);
        }

        SuRF::Iter moveToKeyLessThan(const std::vector<label_t> &key, const bool inclusive) const {
            SuRF::Iter iter = moveToKeyGreaterThan(key, false);
            if (!iter.isValid()) {
                iter = moveToLast();
                return iter;
            }
            if (!iter.getFpFlag()) {
                iter--;
                if (lookupKey(key).has_value())
                    iter--;
            }
            return iter;
        }

        SuRF::Iter moveToKeyLessThan(const Key &key, const bool inclusive) const {
            return moveToKeyLessThan(keyDerivator_(key), inclusive);
        }

        SuRF::Iter moveToFirst() const {
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

        SuRF::Iter moveToLast() const {
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

        std::vector<Value> lookupRange(const Key &left_key,
                         const bool left_inclusive,
                         const Key &right_key,
                         const bool right_inclusive) {

            iter_.clear();
            louds_dense_->moveToKeyGreaterThan(keyDerivator_(left_key), left_inclusive, iter_.dense_iter_);
            if (!iter_.dense_iter_.isValid()) return std::vector<Value>(0);
            if (!iter_.dense_iter_.isComplete()) {
                if (!iter_.dense_iter_.isSearchComplete()) {
                    iter_.passToSparse();
                    louds_sparse_->moveToKeyGreaterThan(keyDerivator_(left_key), left_inclusive, iter_.sparse_iter_);
                    if (!iter_.sparse_iter_.isValid()) {
                        iter_.incrementDenseIter();
                    }
                } else if (!iter_.dense_iter_.isMoveLeftComplete()) {
                    iter_.passToSparse();
                    iter_.sparse_iter_.moveToLeftMostKey();
                }
            }


            std::vector<Value> resultSet = std::vector<Value>(0);
            for (; iter_.isValid(); iter_++) {
                int compare = iter_.compare(keyDerivator_(right_key));

                if (compare == kCouldBePositive ||
                    (right_inclusive && (compare <= 0)) ||
                    (!right_inclusive && (compare < 0))) {
                    Value value = iter_.getValue();
                    resultSet.emplace_back(value);
                }
            }

            return resultSet;
        }

        uint64_t serializedSize() const {
            return (louds_dense_->serializedSize()
                    + louds_sparse_->serializedSize());
        }

        uint64_t getMemoryUsage() const {
            size_t valuesCount = 0;
            for (int i=0; i<values_->size(); i++) {
                valuesCount += (*values_)[i].size();
            }
            return (sizeof(SuRF) + valuesCount * sizeof(Value) + louds_dense_->getMemoryUsage() + louds_sparse_->getMemoryUsage());
        }

        level_t getHeight() const {
            return louds_sparse_->getHeight();
        }

        level_t getSparseStartLevel() const {
            return louds_sparse_->getStartLevel();
        }

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
            surf->louds_dense_ = LoudsDense<Value>::deSerialize(src);
            surf->louds_sparse_ = LoudsSparse<Value>::deSerialize(src);
            surf->iter_ = SuRF::Iter(surf);
            return surf;
        }

        void destroy() {
            louds_dense_->destroy();
            louds_sparse_->destroy();
        }

    private:
        std::optional<Value> lookupKey(const std::vector<label_t> &key) const {
            position_t connect_node_num = 0;
            std::optional<Value> result = std::nullopt;


            result = louds_dense_->lookupKey(key, connect_node_num);
            if (result.has_value() && connect_node_num != 0) {
                result = louds_sparse_->lookupKey(key, connect_node_num);
            }

            return result;
        }

    private:
        LoudsDense<Value> *louds_dense_;
        LoudsSparse<Value> *louds_sparse_;
        SuRFBuilder<Value> *builder_;
        SuRF::Iter iter_;

        std::function<std::vector<label_t>(const Key&)> keyDerivator_;

        shared_ptr<std::vector<std::vector<Value>>> values_;
    };

} // namespace surf

#endif // SURF_H
