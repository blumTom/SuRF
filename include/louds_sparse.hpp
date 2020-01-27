#ifndef LOUDSSPARSE_H_
#define LOUDSSPARSE_H_

#include <string>
#include <algorithm>
#include <functional>

#include "config.hpp"
#include "label_vector.hpp"
#include "value.hpp"
#include "rank.hpp"
#include "select.hpp"
#include "suffix.hpp"
#include "surf_builder.hpp"

namespace surf {

    template<typename Value>
    class LoudsSparse {
    public:
        class Iter {
        public:
            Iter() : is_valid_(false) {};

            Iter(LoudsSparse *trie) : is_valid_(false), trie_(trie), start_node_num_(0),
                                      key_len_(0), is_at_terminator_(false) {
                start_level_ = trie_->getStartLevel();
                for (level_t level = start_level_; level < trie_->getHeight(); level++) {
                    key_.push_back(0);
                    pos_in_trie_.push_back(0);
                }
            }

            void clear() {
                is_valid_ = false;
                key_len_ = 0;
                is_at_terminator_ = false;
            }

            bool isValid() const { return is_valid_; };

            int compare(const std::vector<label_t> &key) const {
                if (is_at_terminator_ && (key_len_ - 1) < (key.size() - start_level_))
                    return -1;
                std::vector<label_t> iter_key = getKey();

                std::vector<label_t> key_sparse;
                for (int i = start_level_; i < std::min(key.size(), iter_key.size() + start_level_); i++) {
                    key_sparse.emplace_back(key[i]);
                }

                int compare = std::memcmp(iter_key.data(), key_sparse.data(), std::min(iter_key.size(), key_sparse.size()));
                if (compare == 0 && iter_key.size() > key_sparse.size()) compare = 1;

                if (compare != 0)
                    return compare;
                position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1]);
                return trie_->suffixes_->compare(suffix_pos, key_sparse, key_len_);
            }

            std::vector<label_t> getKey() const {
                if (!is_valid_)
                    return stringToByteVector(std::string());
                level_t len = key_len_;
                if (is_at_terminator_)
                    len--;

                std::vector<label_t> result;
                for (int i = 0; i < std::min(key_.size(), (size_t) len); i++) {
                    result.emplace_back(key_[i]);
                }
                return result;
            }

            int getSuffix(word_t *suffix) const {
                if ((trie_->suffixes_->getType() == kReal) || (trie_->suffixes_->getType() == kMixed)) {
                    position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1]);
                    *suffix = trie_->suffixes_->readReal(suffix_pos);
                    return trie_->suffixes_->getRealSuffixLen();
                }
                *suffix = 0;
                return 0;
            }

            Value getValue() const {
                position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1]);
                return trie_->getValue(start_level_ + key_len_ - 1, suffix_pos);
            }

            std::vector<label_t> getKeyWithSuffix(unsigned *bitlen) const {
                std::vector<label_t> iter_key = getKey();
                if ((trie_->suffixes_->getType() == kReal) || (trie_->suffixes_->getType() == kMixed)) {
                    position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1]);
                    word_t suffix = trie_->suffixes_->readReal(suffix_pos);
                    if (suffix > 0) {
                        level_t suffix_len = trie_->suffixes_->getRealSuffixLen();
                        *bitlen = suffix_len % 8;
                        suffix <<= (64 - suffix_len);
                        std::vector<label_t> suffix_str = uint64ToByteVector(suffix);
                        unsigned pos = 0;
                        int counter = 0;
                        while (pos < suffix_len) {
                            iter_key.emplace_back(suffix_str[counter]);
                            pos += 8;
                            counter++;
                        }
                    }
                }
                return iter_key;
            }

            position_t getStartNodeNum() const { return start_node_num_; };

            void setStartNodeNum(position_t node_num) { start_node_num_ = node_num; };

            void setToFirstLabelInRoot() {
                assert(start_level_ == 0);
                pos_in_trie_[0] = 0;
                key_[0] = trie_->labels_->read(0);
            }

            void setToLastLabelInRoot() {
                assert(start_level_ == 0);
                pos_in_trie_[0] = trie_->getLastLabelPos(0);
                key_[0] = trie_->labels_->read(pos_in_trie_[0]);
            }

            void moveToLeftMostKey() {
                if (key_len_ == 0) {
                    position_t pos = trie_->getFirstLabelPos(start_node_num_);
                    label_t label = trie_->labels_->read(pos);
                    append(label, pos);
                }

                level_t level = key_len_ - 1;
                position_t pos = pos_in_trie_[level];
                label_t label = trie_->labels_->read(pos);

                if (!trie_->child_indicator_bits_->readBit(pos)) {
                    if ((label == kTerminator)
                        && !trie_->isEndofNode(pos))
                        is_at_terminator_ = true;
                    is_valid_ = true;
                    return;
                }

                while (level < trie_->getHeight()) {
                    position_t node_num = trie_->getChildNodeNum(pos);
                    pos = trie_->getFirstLabelPos(node_num);
                    label = trie_->labels_->read(pos);
                    // if trie branch terminates
                    if (!trie_->child_indicator_bits_->readBit(pos)) {
                        append(label, pos);
                        if ((label == kTerminator)
                            && !trie_->isEndofNode(pos))
                            is_at_terminator_ = true;
                        is_valid_ = true;
                        return;
                    }
                    append(label, pos);
                    level++;
                }
                assert(false); // shouldn't reach here
            }

            void moveToRightMostKey() {
                if (key_len_ == 0) {
                    position_t pos = trie_->getFirstLabelPos(start_node_num_);
                    pos = trie_->getLastLabelPos(start_node_num_);
                    label_t label = trie_->labels_->read(pos);
                    append(label, pos);
                }

                level_t level = key_len_ - 1;
                position_t pos = pos_in_trie_[level];
                label_t label = trie_->labels_->read(pos);

                if (!trie_->child_indicator_bits_->readBit(pos)) {
                    if ((label == kTerminator)
                        && !trie_->isEndofNode(pos))
                        is_at_terminator_ = true;
                    is_valid_ = true;
                    return;
                }

                while (level < trie_->getHeight()) {
                    position_t node_num = trie_->getChildNodeNum(pos);
                    pos = trie_->getLastLabelPos(node_num);
                    label = trie_->labels_->read(pos);
                    // if trie branch terminates
                    if (!trie_->child_indicator_bits_->readBit(pos)) {
                        append(label, pos);
                        if ((label == kTerminator)
                            && !trie_->isEndofNode(pos))
                            is_at_terminator_ = true;
                        is_valid_ = true;
                        return;
                    }
                    append(label, pos);
                    level++;
                }
                assert(false); // shouldn't reach here
            }

            void operator++(int) {
                assert(key_len_ > 0);
                is_at_terminator_ = false;
                position_t pos = pos_in_trie_[key_len_ - 1];
                pos++;
                while (pos >= trie_->louds_bits_->numBits() || trie_->louds_bits_->readBit(pos)) {
                    key_len_--;
                    if (key_len_ == 0) {
                        is_valid_ = false;
                        return;
                    }
                    pos = pos_in_trie_[key_len_ - 1];
                    pos++;
                }
                set(key_len_ - 1, pos);
                return moveToLeftMostKey();
            }

            void operator--(int) {
                assert(key_len_ > 0);
                is_at_terminator_ = false;
                position_t pos = pos_in_trie_[key_len_ - 1];
                if (pos == 0) {
                    is_valid_ = false;
                    return;
                }
                while (trie_->louds_bits_->readBit(pos)) {
                    key_len_--;
                    if (key_len_ == 0) {
                        is_valid_ = false;
                        return;
                    }
                    pos = pos_in_trie_[key_len_ - 1];
                }
                pos--;
                set(key_len_ - 1, pos);
                return moveToRightMostKey();
            }

            std::vector<position_t> getPosition() {
                return pos_in_trie_;
            }

        private:
            void append(const position_t pos) {
                assert(key_len_ < key_.size());
                key_[key_len_] = trie_->labels_->read(pos);
                pos_in_trie_[key_len_] = pos;
                key_len_++;
            }

            void append(const label_t label, const position_t pos) {
                assert(key_len_ < key_.size());
                key_[key_len_] = label;
                pos_in_trie_[key_len_] = pos;
                key_len_++;
            }

            void set(const level_t level, const position_t pos) {
                assert(level < key_.size());
                key_[level] = trie_->labels_->read(pos);
                pos_in_trie_[level] = pos;
            }

        private:
            bool is_valid_; // True means the iter currently points to a valid key
            LoudsSparse *trie_;
            level_t start_level_;
            position_t start_node_num_; // Passed in by the dense iterator; default = 0
            level_t key_len_; // Start counting from start_level_; does NOT include suffix

            std::vector<label_t> key_;
            std::vector<position_t> pos_in_trie_;
            bool is_at_terminator_;

            friend class LoudsSparse;
        };

    public:
        LoudsSparse() {}

        LoudsSparse(const SuRFBuilder<Value> *builder) {
            suffix_type_ = builder->getSuffixType();
            hash_suffix_len_ = builder->getHashSuffixLen();
            real_suffix_len_ = builder->getRealSuffixLen();

            height_ = builder->getLabels().size();
            start_level_ = builder->getSparseStartLevel();

            node_count_dense_ = 0;
            for (level_t level = 0; level < start_level_; level++)
                node_count_dense_ += builder->getNodeCounts()[level];

            if (start_level_ == 0)
                child_count_dense_ = 0;
            else
                child_count_dense_ = node_count_dense_ + builder->getNodeCounts()[start_level_] - 1;

            labels_ = new LabelVector(builder->getLabels(), start_level_, height_);
            values_ = new ValueVector(builder->getValuesPerLevel(), start_level_, height_);

            std::vector<position_t> num_items_per_level;
            for (level_t level = 0; level < height_; level++)
                num_items_per_level.push_back(builder->getLabels()[level].size());

            child_indicator_bits_ = new BitvectorRank(kRankBasicBlockSize, builder->getChildIndicatorBits(),
                                                      num_items_per_level, start_level_, height_);
            louds_bits_ = new BitvectorSelect(kSelectSampleInterval, builder->getLoudsBits(),
                                              num_items_per_level, start_level_, height_);

            if (builder->getSuffixType() == kNone) {
                suffixes_ = new BitvectorSuffix();
            } else {
                level_t hash_suffix_len = builder->getHashSuffixLen();
                level_t real_suffix_len = builder->getRealSuffixLen();
                level_t suffix_len = hash_suffix_len + real_suffix_len;
                std::vector<position_t> num_suffix_bits_per_level;
                for (level_t level = 0; level < height_; level++)
                    num_suffix_bits_per_level.push_back(builder->getSuffixCounts()[level] * suffix_len);

                suffixes_ = new BitvectorSuffix(builder->getSuffixType(), hash_suffix_len, real_suffix_len,
                                                builder->getSuffixes(),
                                                num_suffix_bits_per_level, start_level_, height_);
            }
        }

        ~LoudsSparse() {}

        // point query: trie walk starts at node "in_node_num" instead of root
        // in_node_num is provided by louds-dense's lookupKey function
        std::optional<Value> lookupKey(const std::vector<label_t> &key, const position_t in_node_num) const {
            position_t node_num = in_node_num;
            position_t pos = getFirstLabelPos(node_num);
            level_t level = 0;

            for (level = start_level_; level < key.size(); level++) {
                //child_indicator_bits_->prefetch(pos);
                if (!labels_->search((label_t) key[level], pos, nodeSize(pos)))
                    return std::nullopt;

                // if trie branch terminates
                if (!child_indicator_bits_->readBit(pos)) {
                    if (suffixes_->checkEquality(getSuffixPos(pos), key, level + 1)) {
                        position_t posInLevel = getSuffixPos(pos);
                        return values_->read(posInLevel);
                    } else {
                        return std::nullopt;
                    }
                }

                // move to child
                node_num = getChildNodeNum(pos);
                pos = getFirstLabelPos(node_num);
            }

            if ((labels_->read(pos) == kTerminator) && (!child_indicator_bits_->readBit(pos))) {
                if (suffixes_->checkEquality(getSuffixPos(pos), key, level + 1)) {
                    position_t posInLevel = getSuffixPos(pos);
                    return values_->read(posInLevel);
                } else {
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }

        std::optional<uint64_t> lookupKey(const std::string &key, const position_t in_node_num) const {
            return lookupKey(stringToByteVector(key), in_node_num);
        }

        bool insert(const std::vector<label_t> &key,
                const Value& value,
                position_t &in_node_num,
                label_t &previousLabel,
                word_t &previousSuffix,
                Value &previousValue,
                position_t &previousNodeNum,
                position_t &addedNodes,
                position_t &addedChilds,
                const std::function<std::vector<label_t>(const Value&)> keyDerivator) {
            node_count_dense_ += addedNodes;
            child_count_dense_ += addedChilds;

            position_t nextnodenum = in_node_num;
            bool pushedBack = false;
            if (louds_bits_->numOnes() + node_count_dense_ == nextnodenum) {
                louds_bits_->insert(louds_bits_->numBits(),true);
                louds_bits_->update();
                pushedBack = true;
            }
            position_t pos = getFirstLabelPos(nextnodenum);
            if (!pushedBack) {
                louds_bits_->insert(pos,true);
                louds_bits_->update();
            }
            child_indicator_bits_->insert(pos,false);
            child_indicator_bits_->update();
            labels_->insert(pos,previousLabel);
            values_->insert(getSuffixPos(pos),previousValue);
            suffixes_->insertSuffix(getSuffixPos(pos),previousSuffix);

            return insert(key,value,in_node_num,keyDerivator);
        }

        bool insert(const std::vector<label_t> &key, const Value& value, position_t &in_node_num, const std::function<std::vector<label_t>(const Value&)> keyDerivator) {
            position_t node_num = in_node_num;
            position_t pos = getFirstLabelPos(node_num);
            level_t level = 0;

            for (level = start_level_; level < key.size(); level++) {
                if (!labels_->search((label_t) key[level], pos, nodeSize(pos))) {
                    pos = getFirstLabelPos(node_num);

                    if (labels_->read(pos) == kTerminator) {
                        pos++;
                        louds_bits_->insert(pos,false);
                    } else {
                        bool isSet = louds_bits_->readBit(pos);
                        louds_bits_->unsetBit(pos);
                        louds_bits_->insert(pos,isSet);
                    }

                    labels_->insert(pos,key[level]);
                    child_indicator_bits_->insert(pos,false);
                    child_indicator_bits_->update();
                    word_t key_suffix = suffixes_->constructSuffix(suffix_type_, key, hash_suffix_len_, level + 1, real_suffix_len_);
                    suffixes_->insertSuffix(getSuffixPos(pos),key_suffix);
                    values_->insert(getSuffixPos(pos),value);

                    pos = getFirstLabelPos(node_num);
                    if (!labels_->search((label_t) key[level], pos, nodeSize(pos))) return false;
                    return true;
                }

                // if trie branch terminates
                if (!child_indicator_bits_->readBit(pos)) {
                    position_t suffixPos = getSuffixPos(pos);
                    Value compareValue = values_->read(suffixPos);
                    std::vector<label_t> compareKey = keyDerivator(compareValue);
                    suffixes_->removeSuffix(suffixPos);
                    values_->remove(suffixPos);

                    child_indicator_bits_->setBit(pos);
                    child_indicator_bits_->update();

                    //Move existing value
                    position_t nextnodenum = getChildNodeNum(pos);
                    bool pushedBack = false;
                    if (louds_bits_->numOnes() + node_count_dense_ == nextnodenum) {
                        louds_bits_->insert(louds_bits_->numBits(),true);
                        louds_bits_->update();
                        pushedBack = true;
                    }
                    position_t nextpos = getFirstLabelPos(nextnodenum);
                    if (!pushedBack) {
                        louds_bits_->insert(nextpos,true);
                        louds_bits_->update();
                    }
                    child_indicator_bits_->insert(nextpos,false);
                    child_indicator_bits_->update();
                    word_t compare_key_suffix;
                    if (level < compareKey.size() - 1) {
                        labels_->insert(nextpos,compareKey[level + 1]);
                        values_->insert(getSuffixPos(nextpos),compareValue);
                        compare_key_suffix = suffixes_->constructSuffix(suffix_type_, compareKey, hash_suffix_len_, level + 2, real_suffix_len_);
                        suffixes_->insertSuffix(getSuffixPos(nextpos),compare_key_suffix);
                    } else {
                        labels_->insert(nextpos,kTerminator);
                        values_->insert(getSuffixPos(nextpos),compareValue);
                        compare_key_suffix = suffixes_->constructSuffix(suffix_type_, compareKey, hash_suffix_len_, level + 1, real_suffix_len_);
                        suffixes_->insertSuffix(getSuffixPos(nextpos),compare_key_suffix);
                    }
                }

                // move to child
                node_num = getChildNodeNum(pos);
                pos = getFirstLabelPos(node_num);
            }

            if (labels_->read(pos) != kTerminator) {
                labels_->insert(pos,kTerminator);
                child_indicator_bits_->insert(pos,false);
                child_indicator_bits_->update();
                if (louds_bits_->readBit(pos)) {
                    louds_bits_->unsetBit(pos);
                }
                louds_bits_->insert(pos,true);
                louds_bits_->update();
                word_t insert_suffix = suffixes_->constructSuffix(suffix_type_, key, hash_suffix_len_, level + 1, real_suffix_len_);
                suffixes_->insertSuffix(getSuffixPos(pos),insert_suffix);
                values_->insert(getSuffixPos(pos),value);

                return true;
            }
            return false;
        }

        // return value indicates potential false positive
        bool moveToKeyGreaterThan(const std::vector<label_t> &key,
                                  const bool inclusive, LoudsSparse::Iter &iter) const {
            position_t node_num = iter.getStartNodeNum();
            position_t pos = getFirstLabelPos(node_num);

            level_t level;
            for (level = start_level_; level < key.size(); level++) {
                position_t node_size = nodeSize(pos);
                // if no exact match
                if (!labels_->search((label_t) key[level], pos, node_size)) {
                    moveToLeftInNextSubtrie(pos, node_size, key[level], iter);
                    return false;
                }

                iter.append(key[level], pos);

                // if trie branch terminates
                if (!child_indicator_bits_->readBit(pos))
                    return compareSuffixGreaterThan(pos, key, level + 1, inclusive, iter);

                // move to child
                node_num = getChildNodeNum(pos);
                pos = getFirstLabelPos(node_num);
            }

            if ((labels_->read(pos) == kTerminator)
                && (!child_indicator_bits_->readBit(pos))
                && !isEndofNode(pos)) {
                iter.append(kTerminator, pos);
                iter.is_at_terminator_ = true;
                if (!inclusive)
                    iter++;
                iter.is_valid_ = true;
                return false;
            }

            if (key.size() <= level) {
                iter.moveToLeftMostKey();
                return false;
            }

            iter.is_valid_ = true;
            return true;
        }

        bool moveToKeyGreaterThan(const std::string &key,
                                  const bool inclusive, LoudsSparse::Iter &iter) const {
            return moveToKeyGreaterThan(stringToByteVector(key), inclusive, iter);
        }

        level_t getHeight() const { return height_; };

        level_t getStartLevel() const { return start_level_; };

        uint64_t serializedSize() const {
            uint64_t size = sizeof(height_) + sizeof(start_level_)
                            + sizeof(node_count_dense_) + sizeof(child_count_dense_)
                            + labels_->serializedSize()
                            + child_indicator_bits_->serializedSize()
                            + louds_bits_->serializedSize()
                            + suffixes_->serializedSize();
            sizeAlign(size);
            return size;
        }

        uint64_t getMemoryUsage() const {
            return (sizeof(this)
                    + labels_->size()
                    + child_indicator_bits_->size()
                    + louds_bits_->size()
                    + suffixes_->size());
        }

        Value getValue(level_t level, position_t pos) const {
            return values_->read(pos);
        }

        void serialize(char *&dst) const {
            memcpy(dst, &height_, sizeof(height_));
            dst += sizeof(height_);
            memcpy(dst, &start_level_, sizeof(start_level_));
            dst += sizeof(start_level_);
            memcpy(dst, &node_count_dense_, sizeof(node_count_dense_));
            dst += sizeof(node_count_dense_);
            memcpy(dst, &child_count_dense_, sizeof(child_count_dense_));
            dst += sizeof(child_count_dense_);
            align(dst);
            labels_->serialize(dst);
            child_indicator_bits_->serialize(dst);
            louds_bits_->serialize(dst);
            suffixes_->serialize(dst);
            align(dst);
        }

        static LoudsSparse *deSerialize(char *&src) {
            LoudsSparse *louds_sparse = new LoudsSparse();
            memcpy(&(louds_sparse->height_), src, sizeof(louds_sparse->height_));
            src += sizeof(louds_sparse->height_);
            memcpy(&(louds_sparse->start_level_), src, sizeof(louds_sparse->start_level_));
            src += sizeof(louds_sparse->start_level_);
            memcpy(&(louds_sparse->node_count_dense_), src, sizeof(louds_sparse->node_count_dense_));
            src += sizeof(louds_sparse->node_count_dense_);
            memcpy(&(louds_sparse->child_count_dense_), src, sizeof(louds_sparse->child_count_dense_));
            src += sizeof(louds_sparse->child_count_dense_);
            align(src);
            louds_sparse->labels_ = LabelVector::deSerialize(src);
            louds_sparse->child_indicator_bits_ = BitvectorRank::deSerialize(src);
            louds_sparse->louds_bits_ = BitvectorSelect::deSerialize(src);
            louds_sparse->suffixes_ = BitvectorSuffix::deSerialize(src);
            align(src);
            return louds_sparse;
        }

        void destroy() {
            labels_->destroy();
            child_indicator_bits_->destroy();
            louds_bits_->destroy();
            suffixes_->destroy();
        }

    private:
        position_t getChildNodeNum(const position_t pos) const {
            return (child_indicator_bits_->rank(pos) + child_count_dense_);
        }

        position_t getFirstLabelPos(const position_t node_num) const {
            return louds_bits_->select(node_num + 1 - node_count_dense_);
        }

        position_t getLastLabelPos(const position_t node_num) const {
            position_t next_rank = node_num + 2 - node_count_dense_;
            if (next_rank > louds_bits_->numOnes())
                return (louds_bits_->numBits() - 1);
            return (louds_bits_->select(next_rank) - 1);
        }

        position_t getSuffixPos(const position_t pos) const {
            return (pos - child_indicator_bits_->rank(pos));
        }

        position_t nodeSize(const position_t pos) const {
            assert(louds_bits_->readBit(pos));
            return louds_bits_->distanceToNextSetBit(pos);
        }

        bool isEndofNode(const position_t pos) const {
            return ((pos == louds_bits_->numBits() - 1)
                    || louds_bits_->readBit(pos + 1));
        }

        void moveToLeftInNextSubtrie(position_t pos, const position_t node_size,
                                     const label_t label, LoudsSparse::Iter &iter) const {
            // if no label is greater than key[level] in this node
            if (!labels_->searchGreaterThan(label, pos, node_size)) {
                iter.append(pos + node_size - 1);
                return iter++;
            } else {
                iter.append(pos);
                return iter.moveToLeftMostKey();
            }
        }

        // return value indicates potential false positive
        bool compareSuffixGreaterThan(const position_t pos, const std::vector<label_t> &key,
                                      const level_t level, const bool inclusive,
                                      LoudsSparse::Iter &iter) const {
            position_t suffix_pos = getSuffixPos(pos);
            int compare = suffixes_->compare(suffix_pos, key, level);
            if ((compare != kCouldBePositive) && (compare < 0)) {
                iter++;
                return false;
            }
            iter.is_valid_ = true;
            return true;
        }

    private:
        static const position_t kRankBasicBlockSize = 512;
        static const position_t kSelectSampleInterval = 64;

        level_t height_; // trie height
        level_t start_level_; // louds-sparse encoding starts at this level
        // number of nodes in louds-dense encoding
        position_t node_count_dense_;
        // number of children(1's in child indicator bitmap) in louds-dense encoding
        position_t child_count_dense_;

        LabelVector *labels_;
        BitvectorRank *child_indicator_bits_;
        BitvectorSelect *louds_bits_;
        BitvectorSuffix *suffixes_;

        ValueVector<Value> *values_;

        SuffixType suffix_type_;
        level_t hash_suffix_len_;
        level_t real_suffix_len_;
    };

} // namespace surf

#endif // LOUDSSPARSE_H_
