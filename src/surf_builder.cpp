#include "surf_builder.hpp"

namespace surf {

    void SuRFBuilder::build(const std::vector<std::pair<std::vector<label_t>, uint64_t>> &keys) {
        assert(keys.size() > 0);
        buildSparse(keys);
        if (include_dense_) {
            determineCutoffLevel();
            buildDense();
        }
    }

    void SuRFBuilder::buildSparse(const std::vector<std::pair<std::vector<label_t>, uint64_t>> &keys) {
        for (position_t i = 0; i < keys.size(); i++) {
            level_t level = skipCommonPrefix(keys[i].first);
            position_t curpos = i;

            while ((i + 1 < keys.size()) && isSameKey(keys[curpos].first, keys[i + 1].first,
                                                      std::max(keys[curpos].first.size(), keys[i + 1].first.size())))
                i++;

            if (i < keys.size() - 1) {
                level = insertKeyBytesToTrieUntilUnique(keys[curpos].first, keys[curpos].second, keys[i + 1].first,
                                                        level);
            } else {// for last key, there is no successor key in the list
                level = insertKeyBytesToTrieUntilUnique(keys[curpos].first, keys[curpos].second,
                                                        std::vector<label_t>(0), level);
            }

            insertSuffix(keys[curpos].first, level);
        }
    }

    level_t SuRFBuilder::skipCommonPrefix(const std::vector<label_t> &key) {
        level_t level = 0;
        while (level < key.size() && isCharCommonPrefix((label_t) key[level], level)) {
            setBit(child_indicator_bits_[level], getNumItems(level) - 1);
            level++;
        }
        return level;
    }

    level_t SuRFBuilder::insertKeyBytesToTrieUntilUnique(const std::vector<label_t> &key, const uint64_t value,
                                                         const std::vector<label_t> &next_key,
                                                         const level_t start_level) {
        assert(start_level < key.size());

        level_t level = start_level;
        bool is_start_of_node = false;
        bool is_term = false;
        // If it is the start of level, the louds bit needs to be set.
        if (isLevelEmpty(level)) is_start_of_node = true;

        // After skipping the common prefix, the first following byte
        // shoud be in an the node as the previous key.
        insertKeyByte(key[level], level, is_start_of_node, is_term);
        level++;
        if (level > next_key.size() || !isSameKey(key, next_key, level)) {
            values_[level - 1].push_back(value);
            return level;
        }

        // All the following bytes inserted must be the start of a
        // new node.
        is_start_of_node = true;
        while (level < key.size() && level < next_key.size() && key[level] == next_key[level]) {
            insertKeyByte(key[level], level, is_start_of_node, is_term);
            level++;
        }

        // The last byte inserted makes key unique in the trie.
        if (level < key.size()) {
            insertKeyByte(key[level], level, is_start_of_node, is_term);
        } else {
            is_term = true;
            insertKeyByte(kTerminator, level, is_start_of_node, is_term);
        }
        values_[level].push_back(value);
        level++;

        return level;
    }

    inline void SuRFBuilder::insertSuffix(const std::vector<label_t> &key, const level_t level) {
        if (level >= getTreeHeight())
            addLevel();
        assert(level - 1 < suffixes_.size());
        word_t suffix_word = BitvectorSuffix::constructSuffix(suffix_type_, key, hash_suffix_len_, level,
                                                              real_suffix_len_);
        storeSuffix(level, suffix_word);
    }

    inline bool SuRFBuilder::isCharCommonPrefix(const label_t c, const level_t level) const {
        return (level < getTreeHeight())
               && (!is_last_item_terminator_[level])
               && (c == labels_[level].back());
    }

    inline bool SuRFBuilder::isLevelEmpty(const level_t level) const {
        return (level >= getTreeHeight()) || (labels_[level].size() == 0);
    }

    inline void SuRFBuilder::moveToNextItemSlot(const level_t level) {
        assert(level < getTreeHeight());
        position_t num_items = getNumItems(level);
        if (num_items % kWordSize == 0) {
            child_indicator_bits_[level].push_back(0);
            louds_bits_[level].push_back(0);
        }
    }

    void
    SuRFBuilder::insertKeyByte(const char c, const level_t level, const bool is_start_of_node, const bool is_term) {
        // level should be at most equal to tree height
        if (level >= getTreeHeight())
            addLevel();

        assert(level < getTreeHeight());

        // sets parent node's child indicator
        if (level > 0)
            setBit(child_indicator_bits_[level - 1], getNumItems(level - 1) - 1);

        labels_[level].push_back(c);
        if (is_start_of_node) {
            setBit(louds_bits_[level], getNumItems(level) - 1);
            node_counts_[level]++;
        }
        is_last_item_terminator_[level] = is_term;

        moveToNextItemSlot(level);
    }


    inline void SuRFBuilder::storeSuffix(const level_t level, const word_t suffix) {
        level_t suffix_len = getSuffixLen();
        position_t pos = suffix_counts_[level - 1] * suffix_len;
        assert(pos <= (suffixes_[level - 1].size() * kWordSize));
        if (pos == (suffixes_[level - 1].size() * kWordSize)) suffixes_[level - 1].push_back(0);
        position_t word_id = pos / kWordSize;
        position_t offset = pos % kWordSize;
        position_t word_remaining_len = kWordSize - offset;
        if (suffix_len <= word_remaining_len) {
            word_t shifted_suffix = suffix << (word_remaining_len - suffix_len);
            suffixes_[level - 1][word_id] += shifted_suffix;
        } else {
            word_t suffix_left_part = suffix >> (suffix_len - word_remaining_len);
            suffixes_[level - 1][word_id] += suffix_left_part;
            suffixes_[level - 1].push_back(0);
            word_id++;
            word_t suffix_right_part = suffix << (kWordSize - (suffix_len - word_remaining_len));
            suffixes_[level - 1][word_id] += suffix_right_part;
        }
        suffix_counts_[level - 1]++;
    }

    inline void SuRFBuilder::determineCutoffLevel() {
        level_t cutoff_level = 0;
        uint64_t dense_mem = computeDenseMem(cutoff_level);
        uint64_t sparse_mem = computeSparseMem(cutoff_level);
        while ((cutoff_level < getTreeHeight()) && (dense_mem * sparse_dense_ratio_ < sparse_mem)) {
            cutoff_level++;
            dense_mem = computeDenseMem(cutoff_level);
            sparse_mem = computeSparseMem(cutoff_level);
        }
        sparse_start_level_ = cutoff_level--;
    }

    inline uint64_t SuRFBuilder::computeDenseMem(const level_t downto_level) const {
        assert(downto_level <= getTreeHeight());
        uint64_t mem = 0;
        for (level_t level = 0; level < downto_level; level++) {
            mem += (2 * kFanout * node_counts_[level]);
            if (level > 0) mem += (node_counts_[level - 1] / 8 + 1);
            mem += (suffix_counts_[level] * getSuffixLen() / 8);
        }
        return mem;
    }

    inline uint64_t SuRFBuilder::computeSparseMem(const level_t start_level) const {
        uint64_t mem = 0;
        for (level_t level = start_level; level < getTreeHeight(); level++) {
            position_t num_items = labels_[level].size();
            mem += (num_items + 2 * num_items / 8 + 1);
            mem += (suffix_counts_[level] * getSuffixLen() / 8);
        }
        return mem;
    }

    void SuRFBuilder::buildDense() {
        for (level_t level = 0; level < sparse_start_level_; level++) {
            initDenseVectors(level);
            if (getNumItems(level) == 0) continue;

            position_t node_num = 0;
            if (isTerminator(level, 0))
                setBit(prefixkey_indicator_bits_[level], 0);
            else
                setLabelAndChildIndicatorBitmap(level, node_num, 0);
            for (position_t pos = 1; pos < getNumItems(level); pos++) {
                if (isStartOfNode(level, pos)) {
                    node_num++;
                    if (isTerminator(level, pos)) {
                        setBit(prefixkey_indicator_bits_[level], node_num);
                        continue;
                    }
                }
                setLabelAndChildIndicatorBitmap(level, node_num, pos);
            }
        }
    }

    void SuRFBuilder::initDenseVectors(const level_t level) {
        bitmap_labels_.push_back(std::vector<word_t>());
        bitmap_child_indicator_bits_.push_back(std::vector<word_t>());
        prefixkey_indicator_bits_.push_back(std::vector<word_t>());

        for (position_t nc = 0; nc < node_counts_[level]; nc++) {
            for (int i = 0; i < (int) kFanout; i += kWordSize) {
                bitmap_labels_[level].push_back(0);
                bitmap_child_indicator_bits_[level].push_back(0);
            }
            if (nc % kWordSize == 0)
                prefixkey_indicator_bits_[level].push_back(0);
        }
    }

    void SuRFBuilder::setLabelAndChildIndicatorBitmap(const level_t level,
                                                      const position_t node_num, const position_t pos) {
        label_t label = labels_[level][pos];
        setBit(bitmap_labels_[level], node_num * kFanout + label);
        if (readBit(child_indicator_bits_[level], pos))
            setBit(bitmap_child_indicator_bits_[level], node_num * kFanout + label);
    }

    void SuRFBuilder::addLevel() {
        values_.push_back(std::vector<uint64_t>());
        labels_.push_back(std::vector<label_t>());
        child_indicator_bits_.push_back(std::vector<word_t>());
        louds_bits_.push_back(std::vector<word_t>());
        suffixes_.push_back(std::vector<word_t>());
        suffix_counts_.push_back(0);

        node_counts_.push_back(0);
        is_last_item_terminator_.push_back(false);

        child_indicator_bits_[getTreeHeight() - 1].push_back(0);
        louds_bits_[getTreeHeight() - 1].push_back(0);
    }

    position_t SuRFBuilder::getNumItems(const level_t level) const {
        return labels_[level].size();
    }

    bool SuRFBuilder::isStartOfNode(const level_t level, const position_t pos) const {
        return readBit(louds_bits_[level], pos);
    }

    bool SuRFBuilder::isTerminator(const level_t level, const position_t pos) const {
        label_t label = labels_[level][pos];
        return ((label == kTerminator) && !readBit(child_indicator_bits_[level], pos));
    }

}