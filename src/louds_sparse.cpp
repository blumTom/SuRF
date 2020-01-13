#include "louds_sparse.hpp"

namespace surf {

    LoudsSparse::LoudsSparse(const SuRFBuilder *builder) {
        values_ = builder->getValues();

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

    std::optional<uint64_t> LoudsSparse::lookupKey(const std::string &key, const position_t in_node_num) const {
        return lookupKey(stringToByteVector(key), in_node_num);
    }

    std::optional<uint64_t>
    LoudsSparse::lookupKey(const std::vector<label_t> &key, const position_t in_node_num) const {
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
                    return getValue(level, posInLevel);
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
                return getValue(level, posInLevel);
            } else {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    bool LoudsSparse::moveToKeyGreaterThan(const std::vector<label_t> &key,
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

    bool LoudsSparse::moveToKeyGreaterThan(const std::string &key,
                                           const bool inclusive, LoudsSparse::Iter &iter) const {
        return moveToKeyGreaterThan(stringToByteVector(key), inclusive, iter);
    }

    uint64_t LoudsSparse::serializedSize() const {
        uint64_t size = sizeof(height_) + sizeof(start_level_)
                        + sizeof(node_count_dense_) + sizeof(child_count_dense_)
                        + labels_->serializedSize()
                        + child_indicator_bits_->serializedSize()
                        + louds_bits_->serializedSize()
                        + suffixes_->serializedSize();
        sizeAlign(size);
        return size;
    }

    uint64_t LoudsSparse::getMemoryUsage() const {
        return (sizeof(this)
                + labels_->size()
                + child_indicator_bits_->size()
                + louds_bits_->size()
                + suffixes_->size());
    }

    position_t LoudsSparse::getChildNodeNum(const position_t pos) const {
        return (child_indicator_bits_->rank(pos) + child_count_dense_);
    }

    position_t LoudsSparse::getFirstLabelPos(const position_t node_num) const {
        return louds_bits_->select(node_num + 1 - node_count_dense_);
    }

    position_t LoudsSparse::getLastLabelPos(const position_t node_num) const {
        position_t next_rank = node_num + 2 - node_count_dense_;
        if (next_rank > louds_bits_->numOnes())
            return (louds_bits_->numBits() - 1);
        return (louds_bits_->select(next_rank) - 1);
    }

    position_t LoudsSparse::getSuffixPos(const position_t pos) const {
        return (pos - child_indicator_bits_->rank(pos));
    }

    position_t LoudsSparse::nodeSize(const position_t pos) const {
        assert(louds_bits_->readBit(pos));
        return louds_bits_->distanceToNextSetBit(pos);
    }

    bool LoudsSparse::isEndofNode(const position_t pos) const {
        return ((pos == louds_bits_->numBits() - 1)
                || louds_bits_->readBit(pos + 1));
    }

    void LoudsSparse::moveToLeftInNextSubtrie(position_t pos, const position_t node_size,
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

    bool LoudsSparse::compareSuffixGreaterThan(const position_t pos, const std::vector<label_t> &key,
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

//============================================================================

    void LoudsSparse::Iter::clear() {
        is_valid_ = false;
        key_len_ = 0;
        is_at_terminator_ = false;
    }

    int LoudsSparse::Iter::compare(const std::vector<label_t> &key) const {
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


    std::vector<label_t> LoudsSparse::Iter::getKey() const {
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

    int LoudsSparse::Iter::getSuffix(word_t *suffix) const {
        if ((trie_->suffixes_->getType() == kReal) || (trie_->suffixes_->getType() == kMixed)) {
            position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1]);
            *suffix = trie_->suffixes_->readReal(suffix_pos);
            return trie_->suffixes_->getRealSuffixLen();
        }
        *suffix = 0;
        return 0;
    }

    uint64_t LoudsSparse::Iter::getValue() const {
        position_t suffix_pos = trie_->getSuffixPos(pos_in_trie_[key_len_ - 1]);
        return trie_->getValue(start_level_ + key_len_ - 1, suffix_pos);
    }

    std::vector<label_t> LoudsSparse::Iter::getKeyWithSuffix(unsigned *bitlen) const {
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

    void LoudsSparse::Iter::append(const position_t pos) {
        assert(key_len_ < key_.size());
        key_[key_len_] = trie_->labels_->read(pos);
        pos_in_trie_[key_len_] = pos;
        key_len_++;
    }

    void LoudsSparse::Iter::append(const label_t label, const position_t pos) {
        assert(key_len_ < key_.size());
        key_[key_len_] = label;
        pos_in_trie_[key_len_] = pos;
        key_len_++;
    }

    void LoudsSparse::Iter::set(const level_t level, const position_t pos) {
        assert(level < key_.size());
        key_[level] = trie_->labels_->read(pos);
        pos_in_trie_[level] = pos;
    }

    void LoudsSparse::Iter::setToFirstLabelInRoot() {
        assert(start_level_ == 0);
        pos_in_trie_[0] = 0;
        key_[0] = trie_->labels_->read(0);
    }

    void LoudsSparse::Iter::setToLastLabelInRoot() {
        assert(start_level_ == 0);
        pos_in_trie_[0] = trie_->getLastLabelPos(0);
        key_[0] = trie_->labels_->read(pos_in_trie_[0]);
    }

    void LoudsSparse::Iter::moveToLeftMostKey() {
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

    void LoudsSparse::Iter::moveToRightMostKey() {
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

    void LoudsSparse::Iter::operator++(int) {
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

    void LoudsSparse::Iter::operator--(int) {
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

}