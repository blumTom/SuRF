#ifndef LOUDSDENSE_H_
#define LOUDSDENSE_H_

#include <string>
#include <algorithm>

#include "config.hpp"
#include "rank.hpp"
#include "suffix.hpp"
#include "surf_builder.hpp"

namespace surf {

    template<typename Value>
    class LoudsDense {
    public:
        class Iter {
        public:
            Iter() : is_valid_(false) {};

            Iter(LoudsDense *trie) : is_valid_(false), is_search_complete_(false),
                                     is_move_left_complete_(false),
                                     is_move_right_complete_(false),
                                     trie_(trie),
                                     send_out_node_num_(0), key_len_(0),
                                     is_at_prefix_key_(false) {
                for (level_t level = 0; level < trie_->getHeight(); level++) {
                    key_.push_back(0);
                    pos_in_trie_.push_back(0);
                }
            }

            void clear() {
                is_valid_ = false;
                key_len_ = 0;
                is_at_prefix_key_ = false;
            }

            bool isValid() const { return is_valid_; };

            bool isSearchComplete() const { return is_search_complete_; };

            bool isMoveLeftComplete() const { return is_move_left_complete_; };

            bool isMoveRightComplete() const { return is_move_right_complete_; };

            bool isComplete() const {
                return (is_search_complete_ &&
                        (is_move_left_complete_ && is_move_right_complete_));
            }

            int compare(const std::vector<label_t> &key) const {
                if (is_at_prefix_key_ && (key_len_ - 1) < key.size())
                    return -1;
                std::vector<label_t> iter_key = getKey();

                std::vector<label_t> key_dense;
                for (int i = 0; i < std::min(key.size(), iter_key.size()); i++) {
                    key_dense.emplace_back(key[i]);
                }

                int compare = std::memcmp(iter_key.data(), key_dense.data(), std::min(iter_key.size(), key_dense.size()));
                if (compare == 0 && iter_key.size() > key_dense.size()) compare = 1;

                if (compare != 0) return compare;
                if (isComplete()) {
                    position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1], is_at_prefix_key_);
                    return trie_->suffixes_->compare(suffix_pos, key, key_len_);
                }
                return compare;
            }

            std::vector<label_t> getKey() const {
                if (!is_valid_)
                    return std::vector<label_t>(0);
                level_t len = key_len_;
                if (is_at_prefix_key_)
                    len--;

                std::vector<label_t> result;
                for (int i = 0; i < std::min(key_.size(), (size_t) len); i++) {
                    result.emplace_back(key_[i]);
                }
                return result;
            }

            int getSuffix(word_t *suffix) const {
                if (isComplete()
                    && ((trie_->suffixes_->getType() == kReal) || (trie_->suffixes_->getType() == kMixed))) {
                    position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1], is_at_prefix_key_);
                    *suffix = trie_->suffixes_->readReal(suffix_pos);
                    return trie_->suffixes_->getRealSuffixLen();
                }
                *suffix = 0;
                return 0;
            }

            Value getValue() const {
                if (isComplete()) {
                    position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1], is_at_prefix_key_);
                    return trie_->getValue(key_len_ - 1, suffix_pos);
                }
                return 0;
            }

            std::vector<label_t> getKeyWithSuffix(unsigned *bitlen) const {
                std::vector<label_t> iter_key = getKey();
                if (isComplete()
                    && ((trie_->suffixes_->getType() == kReal) || (trie_->suffixes_->getType() == kMixed))) {
                    position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1], is_at_prefix_key_);
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
                            counter++;
                            pos += 8;
                        }
                    }
                }
                return iter_key;
            }

            position_t getSendOutNodeNum() const { return send_out_node_num_; };

            void setToFirstLabelInRoot() {
                if (trie_->label_bitmaps_->readBit(0)) {
                    pos_in_trie_[0] = 0;
                    key_[0] = (label_t) 0;
                } else {
                    pos_in_trie_[0] = trie_->getNextPos(0);
                    key_[0] = (label_t) pos_in_trie_[0];
                }
                key_len_++;
            }

            void setToLastLabelInRoot() {
                bool is_out_of_bound;
                pos_in_trie_[0] = trie_->getPrevPos(kNodeFanout, &is_out_of_bound);
                key_[0] = (label_t) pos_in_trie_[0];
                key_len_++;
            }

            void moveToLeftMostKey() {
                assert(key_len_ > 0);
                level_t level = key_len_ - 1;
                position_t pos = pos_in_trie_[level];
                if (!trie_->child_indicator_bitmaps_->readBit(pos))
                    // valid, search complete, moveLeft complete, moveRight complete
                    return setFlags(true, true, true, true);

                while (level < trie_->getHeight() - 1) {
                    position_t node_num = trie_->getChildNodeNum(pos);
                    //if the current prefix is also a key
                    if (trie_->prefixkey_indicator_bits_->readBit(node_num)) {
                        append(trie_->getNextPos(node_num * kNodeFanout - 1));
                        is_at_prefix_key_ = true;
                        // valid, search complete, moveLeft complete, moveRight complete
                        return setFlags(true, true, true, true);
                    }

                    pos = trie_->getNextPos(node_num * kNodeFanout - 1);
                    append(pos);

                    // if trie branch terminates
                    if (!trie_->child_indicator_bitmaps_->readBit(pos))
                        // valid, search complete, moveLeft complete, moveRight complete
                        return setFlags(true, true, true, true);

                    level++;
                }
                send_out_node_num_ = trie_->getChildNodeNum(pos);
                // valid, search complete, moveLeft INCOMPLETE, moveRight complete
                setFlags(true, true, false, true);
            }

            void moveToRightMostKey() {
                assert(key_len_ > 0);
                level_t level = key_len_ - 1;
                position_t pos = pos_in_trie_[level];
                if (!trie_->child_indicator_bitmaps_->readBit(pos))
                    // valid, search complete, moveLeft complete, moveRight complete
                    return setFlags(true, true, true, true);

                while (level < trie_->getHeight() - 1) {
                    position_t node_num = trie_->getChildNodeNum(pos);
                    bool is_out_of_bound;
                    pos = trie_->getPrevPos((node_num + 1) * kNodeFanout, &is_out_of_bound);
                    if (is_out_of_bound) {
                        is_valid_ = false;
                        return;
                    }
                    append(pos);

                    // if trie branch terminates
                    if (!trie_->child_indicator_bitmaps_->readBit(pos))
                        // valid, search complete, moveLeft complete, moveRight complete
                        return setFlags(true, true, true, true);

                    level++;
                }
                send_out_node_num_ = trie_->getChildNodeNum(pos);
                // valid, search complete, moveleft complete, moveRight INCOMPLETE
                setFlags(true, true, true, false);
            }

            void operator++(int) {
                assert(key_len_ > 0);
                if (is_at_prefix_key_) {
                    is_at_prefix_key_ = false;
                    return moveToLeftMostKey();
                }
                position_t pos = pos_in_trie_[key_len_ - 1];
                position_t next_pos = trie_->getNextPos(pos);
                // if crossing node boundary
                while ((next_pos / kNodeFanout) > (pos / kNodeFanout)) {
                    key_len_--;
                    if (key_len_ == 0) {
                        is_valid_ = false;
                        return;
                    }
                    pos = pos_in_trie_[key_len_ - 1];
                    next_pos = trie_->getNextPos(pos);
                }
                set(key_len_ - 1, next_pos);
                return moveToLeftMostKey();
            }

            void operator--(int) {
                assert(key_len_ > 0);
                if (is_at_prefix_key_) {
                    is_at_prefix_key_ = false;
                    key_len_--;
                }
                position_t pos = pos_in_trie_[key_len_ - 1];
                bool is_out_of_bound;
                position_t prev_pos = trie_->getPrevPos(pos, &is_out_of_bound);
                if (is_out_of_bound) {
                    is_valid_ = false;
                    return;
                }

                // if crossing node boundary
                while ((prev_pos / kNodeFanout) < (pos / kNodeFanout)) {
                    //if the current prefix is also a key
                    position_t node_num = pos / kNodeFanout;
                    if (trie_->prefixkey_indicator_bits_->readBit(node_num)) {
                        is_at_prefix_key_ = true;
                        // valid, search complete, moveLeft complete, moveRight complete
                        return setFlags(true, true, true, true);
                    }

                    key_len_--;
                    if (key_len_ == 0) {
                        is_valid_ = false;
                        return;
                    }
                    pos = pos_in_trie_[key_len_ - 1];
                    prev_pos = trie_->getPrevPos(pos, &is_out_of_bound);
                    if (is_out_of_bound) {
                        is_valid_ = false;
                        return;
                    }
                }
                set(key_len_ - 1, prev_pos);
                return moveToRightMostKey();
            }

            std::vector<position_t> getPositions() {
                return pos_in_trie_;
            }

        private:
            inline void append(position_t pos) {
                assert(key_len_ < key_.size());
                key_[key_len_] = (label_t) (pos % kNodeFanout);
                pos_in_trie_[key_len_] = pos;
                key_len_++;
            }

            inline void set(level_t level, position_t pos) {
                assert(level < key_.size());
                key_[level] = (label_t) (pos % kNodeFanout);
                pos_in_trie_[level] = pos;
            }

            inline void setSendOutNodeNum(position_t node_num) { send_out_node_num_ = node_num; };

            inline void setFlags(const bool is_valid, const bool is_search_complete,
                                 const bool is_move_left_complete,
                                 const bool is_move_right_complete) {
                is_valid_ = is_valid;
                is_search_complete_ = is_search_complete;
                is_move_left_complete_ = is_move_left_complete;
                is_move_right_complete_ = is_move_right_complete;
            }

        private:
            // True means the iter either points to a valid key
            // or to a prefix with length trie_->getHeight()
            bool is_valid_;
            // If false, call moveToKeyGreaterThan in LoudsSparse to complete
            bool is_search_complete_;
            // If false, call moveToLeftMostKey in LoudsSparse to complete
            bool is_move_left_complete_;
            // If false, call moveToRightMostKey in LoudsSparse to complete
            bool is_move_right_complete_;
            LoudsDense *trie_;
            position_t send_out_node_num_;
            level_t key_len_; // Does NOT include suffix

            std::vector<label_t> key_;
            std::vector<position_t> pos_in_trie_;
            bool is_at_prefix_key_;

            friend class LoudsDense;
        };

    public:
        LoudsDense() {}

        LoudsDense(const SuRFBuilder<Value> *builder) {
            suffix_type_ = builder->getSuffixType();
            hash_suffix_len_ = builder->getHashSuffixLen();
            real_suffix_len_ = builder->getRealSuffixLen();

            values_ = builder->getValues();

            height_ = builder->getSparseStartLevel();
            std::vector<position_t> num_bits_per_level;
            for (level_t level = 0; level < height_; level++)
                num_bits_per_level.push_back(builder->getBitmapLabels()[level].size() * kWordSize);

            label_bitmaps_ = new BitvectorRank(kRankBasicBlockSize, builder->getBitmapLabels(),
                                               num_bits_per_level, 0, height_);
            child_indicator_bitmaps_ = new BitvectorRank(kRankBasicBlockSize,
                                                         builder->getBitmapChildIndicatorBits(),
                                                         num_bits_per_level, 0, height_);
            prefixkey_indicator_bits_ = new BitvectorRank(kRankBasicBlockSize,
                                                          builder->getPrefixkeyIndicatorBits(),
                                                          builder->getNodeCounts(), 0, height_);

            if (builder->getSuffixType() == kNone) {
                suffixes_ = new BitvectorSuffix();
            } else {
                level_t hash_suffix_len = builder->getHashSuffixLen();
                level_t real_suffix_len = builder->getRealSuffixLen();
                level_t suffix_len = hash_suffix_len + real_suffix_len;
                std::vector<position_t> num_suffix_bits_per_level;
                for (level_t level = 0; level < height_; level++)
                    num_suffix_bits_per_level.push_back(builder->getSuffixCounts()[level] * suffix_len);
                suffixes_ = new BitvectorSuffix(builder->getSuffixType(),
                                                hash_suffix_len, real_suffix_len,
                                                builder->getSuffixes(),
                                                num_suffix_bits_per_level, 0, height_);
            }
        }

        ~LoudsDense() {}

        // Returns whether key exists in the trie so far
        // out_node_num == 0 means search terminates in louds-dense.
        std::optional<Value> lookupKey(const std::vector<label_t> &key, position_t &out_node_num) const {
            position_t node_num = 0;
            position_t pos = 0;
            for (level_t level = 0; level < height_; level++) {
                pos = (node_num * kNodeFanout);
                if (level >= key.size()) { //if run out of searchKey bytes
                    if (prefixkey_indicator_bits_->readBit(node_num)) { //if the prefix is also a key
                        if (suffixes_->checkEquality(getSuffixPos(pos, true), key, level + 1)) {
                            position_t posInLevel = getSuffixPos(pos, true);
                            return getValue(level, posInLevel);
                        } else {
                            return std::nullopt;
                        }
                    } else {
                        return std::nullopt;
                    }
                }
                pos += (label_t) key[level];

                if (!label_bitmaps_->readBit(pos)) { //if key byte does not exist
                    return std::nullopt;
                }
                if (!child_indicator_bitmaps_->readBit(pos)) { //if trie branch terminates
                    if (suffixes_->checkEquality(getSuffixPos(pos, false), key, level + 1)) {
                        position_t posInLevel = getSuffixPos(pos, false);
                        return getValue(level, posInLevel);
                    } else {
                        return std::nullopt;
                    }
                }

                node_num = getChildNodeNum(pos);
            }
            //search will continue in LoudsSparse
            out_node_num = node_num;
            return 0;
        }

        std::optional<uint64_t> lookupKey(const std::string &key, position_t &out_node_num) const {
            return lookupKey(stringToByteVector(key), out_node_num);
        }

        int insert(const std::vector<label_t> &key, const Value& value, position_t &out_node_num, label_t &previouslabel, word_t &previoussuffix, Value &previousValue, position_t &previousNodeNum, position_t &addedNodes, position_t &addedChilds, const std::function<std::vector<label_t>(const Value&)> keyDerivator) const {
            int returnCode = 1;

            position_t node_num = 0;
            position_t pos = 0;
            for (level_t level = 0; level < height_; level++) {
                pos = (node_num * kNodeFanout);
                if (level >= key.size()) { //if run out of searchKey bytes
                    if (prefixkey_indicator_bits_->readBit(node_num)) { //if the prefix is also a key
                        return -1;
                    } else {
                        prefixkey_indicator_bits_->setBit(node_num);
                        prefixkey_indicator_bits_->update();
                        word_t insert_suffix = suffixes_->constructSuffix(suffix_type_, key, hash_suffix_len_, level + 1, real_suffix_len_);
                        suffixes_->insertSuffix(getSuffixPos(pos,true),insert_suffix);
                        insertValue(level,getSuffixPos(pos,true),value);
                        return 0;
                    }
                }

                pos += (label_t) key[level];

                if (!label_bitmaps_->readBit(pos)) { //if key byte does not exist
                    label_bitmaps_->setBit(pos);
                    label_bitmaps_->update();
                    child_indicator_bitmaps_->unsetBit(pos);
                    child_indicator_bitmaps_->update();
                    word_t insert_suffix = suffixes_->constructSuffix(suffix_type_, key, hash_suffix_len_, level + 1, real_suffix_len_);
                    suffixes_->insertSuffix(getSuffixPos(pos,false),insert_suffix);
                    insertValue(level,getSuffixPos(pos,false),value);

                    return 0;
                }

                if (!child_indicator_bitmaps_->readBit(pos)) { //if trie branch terminates
                    position_t suffixPos = getSuffixPos(pos,false);
                    Value compareValue = getValue(level, suffixPos);
                    std::vector<label_t> compareKey = keyDerivator(compareValue);
                    suffixes_->removeSuffix(suffixPos);
                    removeValue(level,suffixPos);

                    child_indicator_bitmaps_->setBit(pos);
                    child_indicator_bitmaps_->update();

                    if (level >= height_ - 1) {
                        if (level < compareKey.size() - 1) {
                            previouslabel = compareKey[level + 1];
                            previoussuffix = suffixes_->constructSuffix(suffix_type_, compareKey, hash_suffix_len_, level + 2, real_suffix_len_);
                            previousValue = compareValue;
                            if (level < key.size() - 1 && compareKey[level+1] < key[level + 1]) previousNodeNum--;
                            addedChilds++;
                        } else {
                            previouslabel = kTerminator;
                            previoussuffix = suffixes_->constructSuffix(suffix_type_, compareKey, hash_suffix_len_, level + 1, real_suffix_len_);
                            previousValue = compareValue;
                            addedChilds++;
                        }
                        returnCode = 2;
                    } else {
                        position_t nextnodenum = getChildNodeNum(pos);
                        position_t nextpos = (nextnodenum * kNodeFanout);
                        label_bitmaps_->insertWords(nextpos, 4);
                        label_bitmaps_->update();
                        child_indicator_bitmaps_->insertWords(nextpos, 4);
                        child_indicator_bitmaps_->update();
                        prefixkey_indicator_bits_->insert(nextpos,false);
                        prefixkey_indicator_bits_->insert(nextpos,false);
                        prefixkey_indicator_bits_->insert(nextpos,false);
                        prefixkey_indicator_bits_->insert(nextpos,false);
                        prefixkey_indicator_bits_->update();
                        nextpos += compareKey[level + 1];
                        if (level < compareKey.size() - 1) {
                            label_bitmaps_->setBit(nextpos);
                            label_bitmaps_->update();
                            word_t compareSuffix = suffixes_->constructSuffix(suffix_type_, compareKey, hash_suffix_len_, level + 1, real_suffix_len_);
                            suffixes_->insertSuffix(getSuffixPos(nextpos,false),compareSuffix);
                            insertValue(level + 1,getSuffixPos(nextpos,false),compareValue);
                        } else {
                            prefixkey_indicator_bits_->setBit(nextnodenum);
                            prefixkey_indicator_bits_->update();
                            word_t compareSuffix = previoussuffix = suffixes_->constructSuffix(suffix_type_, compareKey, hash_suffix_len_, level + 1, real_suffix_len_);
                            suffixes_->insertSuffix(getSuffixPos(nextpos,false),compareSuffix);
                            insertValue(level + 1,getSuffixPos(nextpos,false),compareValue);
                        }
                        addedNodes++;
                        addedChilds++;
                    }
                }

                node_num = getChildNodeNum(pos);
            }
            //search will continue in LoudsSparse
            out_node_num = node_num;
            return returnCode;
        }

        void insertValue(level_t level, position_t pos, const Value& value) const {
            if (values_->size() <= level) (*values_).emplace_back(std::vector<Value>(0));
            int diff = 0;
            for (int i=0;i<level; i++) {
                diff += (*values_)[i].size();
            }
            pos -= diff;
            assert((*values_)[level].size() > pos);
            (*values_)[level].insert((*values_)[level].begin() + pos,value);
        }

        void removeValue(level_t level, position_t pos) const {
            int diff = 0;
            for (int i=0;i<level; i++) {
                diff += (*values_)[i].size();
            }
            pos -= diff;
            assert((*values_)[level].size() > pos);
            (*values_)[level].erase((*values_)[level].begin() + pos);
        }

        // return value indicates potential false positive
        bool moveToKeyGreaterThan(const std::vector<label_t> &key,
                                  const bool inclusive, LoudsDense::Iter &iter) const {
            position_t node_num = 0;
            position_t pos = 0;
            for (level_t level = 0; level < height_; level++) {
                // if is_at_prefix_key_, pos is at the next valid position in the child node
                pos = node_num * kNodeFanout;
                if (level >= key.size()) { // if run out of searchKey bytes
                    iter.append(getNextPos(pos - 1));
                    if (prefixkey_indicator_bits_->readBit(node_num)) //if the prefix is also a key
                        iter.is_at_prefix_key_ = true;
                    else
                        iter.moveToLeftMostKey();
                    // valid, search complete, moveLeft complete, moveRight complete
                    iter.setFlags(true, true, true, true);
                    return true;
                }

                pos += (label_t) key[level];
                iter.append(pos);

                // if no exact match
                if (!label_bitmaps_->readBit(pos)) {
                    iter++;
                    return false;
                }
                //if trie branch terminates
                if (!child_indicator_bitmaps_->readBit(pos))
                    return compareSuffixGreaterThan(pos, key, level + 1, inclusive, iter);
                node_num = getChildNodeNum(pos);
            }

            //search will continue in LoudsSparse
            iter.setSendOutNodeNum(node_num);
            // valid, search INCOMPLETE, moveLeft complete, moveRight complete
            iter.setFlags(true, false, true, true);
            return true;
        }

        bool moveToKeyGreaterThan(const std::string &key,
                                  const bool inclusive, LoudsDense::Iter &iter) const {
            return moveToKeyGreaterThan(stringToByteVector(key), inclusive, iter);
        }

        uint64_t getHeight() const { return height_; };

        uint64_t serializedSize() const {
            uint64_t size = sizeof(height_)
                            + label_bitmaps_->serializedSize()
                            + child_indicator_bitmaps_->serializedSize()
                            + prefixkey_indicator_bits_->serializedSize()
                            + suffixes_->serializedSize();
            sizeAlign(size);
            return size;
        }

        uint64_t getMemoryUsage() const {
            return (sizeof(LoudsDense)
                    + label_bitmaps_->size()
                    + child_indicator_bitmaps_->size()
                    + prefixkey_indicator_bits_->size()
                    + suffixes_->size());
        }

        Value getValue(level_t level, position_t pos) const {
            assert(values_->size() > level);
            int diff = 0;
            for (int i=0;i<level; i++) {
                diff += (*values_)[i].size();
            }
            pos -= diff;
            assert((*values_)[level].size() > pos);
            return (*values_)[level][pos];
        }

        void serialize(char *&dst) const {
            memcpy(dst, &height_, sizeof(height_));
            dst += sizeof(height_);
            align(dst);
            label_bitmaps_->serialize(dst);
            child_indicator_bitmaps_->serialize(dst);
            prefixkey_indicator_bits_->serialize(dst);
            suffixes_->serialize(dst);
            align(dst);
        }

        static LoudsDense *deSerialize(char *&src) {
            LoudsDense *louds_dense = new LoudsDense();
            memcpy(&(louds_dense->height_), src, sizeof(louds_dense->height_));
            src += sizeof(louds_dense->height_);
            align(src);
            louds_dense->label_bitmaps_ = BitvectorRank::deSerialize(src);
            louds_dense->child_indicator_bitmaps_ = BitvectorRank::deSerialize(src);
            louds_dense->prefixkey_indicator_bits_ = BitvectorRank::deSerialize(src);
            louds_dense->suffixes_ = BitvectorSuffix::deSerialize(src);
            align(src);
            return louds_dense;
        }

        void destroy() {
            label_bitmaps_->destroy();
            child_indicator_bitmaps_->destroy();
            prefixkey_indicator_bits_->destroy();
            suffixes_->destroy();
        }

    private:
        position_t getChildNodeNum(const position_t pos) const {
            return child_indicator_bitmaps_->rank(pos);
        }

        position_t getSuffixPos(const position_t pos, const bool is_prefix_key) const {
            position_t node_num = pos / kNodeFanout;
            position_t suffix_pos = (label_bitmaps_->rank(pos)
                                     - child_indicator_bitmaps_->rank(pos)
                                     + prefixkey_indicator_bits_->rank(node_num)
                                     - 1);
            if (is_prefix_key && label_bitmaps_->readBit(pos) && !child_indicator_bitmaps_->readBit(pos))
                suffix_pos--;
            return suffix_pos;
        }

        position_t getNextPos(const position_t pos) const {
            return pos + label_bitmaps_->distanceToNextSetBit(pos);
        }

        position_t getPrevPos(const position_t pos, bool *is_out_of_bound) const {
            position_t distance = label_bitmaps_->distanceToPrevSetBit(pos);
            if (pos <= distance) {
                *is_out_of_bound = true;
                return 0;
            }
            *is_out_of_bound = false;
            return (pos - distance);
        }

        bool compareSuffixGreaterThan(const position_t pos, const std::vector<label_t> &key,
                                      const level_t level, const bool inclusive,
                                      LoudsDense::Iter &iter) const {
            position_t suffix_pos = getSuffixPos(pos, false);
            int compare = suffixes_->compare(suffix_pos, key, level);
            if ((compare != kCouldBePositive) && (compare < 0)) {
                iter++;
                return false;
            }
            // valid, search complete, moveLeft complete, moveRight complete
            iter.setFlags(true, true, true, true);
            return true;
        }

    private:
        static const position_t kNodeFanout = 256;
        static const position_t kRankBasicBlockSize = 512;

        level_t height_;

        BitvectorRank *label_bitmaps_;
        BitvectorRank *child_indicator_bitmaps_;
        BitvectorRank *prefixkey_indicator_bits_; //1 bit per internal node
        BitvectorSuffix *suffixes_;

        shared_ptr<std::vector<std::vector<Value>>> values_;

        SuffixType suffix_type_;
        level_t hash_suffix_len_;
        level_t real_suffix_len_;
    };

} //namespace surf

#endif // LOUDSDENSE_H_
