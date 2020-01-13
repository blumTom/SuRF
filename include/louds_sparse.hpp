#ifndef LOUDSSPARSE_H_
#define LOUDSSPARSE_H_

#include <string>
#include <algorithm>

#include "config.hpp"
#include "label_vector.hpp"
#include "rank.hpp"
#include "select.hpp"
#include "suffix.hpp"
#include "surf_builder.hpp"

namespace surf {

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

            void clear();

            bool isValid() const { return is_valid_; };

            int compare(const std::vector<label_t> &key) const;

            std::vector<label_t> getKey() const;

            int getSuffix(word_t *suffix) const;

            uint64_t getValue() const;

            std::vector<label_t> getKeyWithSuffix(unsigned *bitlen) const;

            position_t getStartNodeNum() const { return start_node_num_; };

            void setStartNodeNum(position_t node_num) { start_node_num_ = node_num; };

            void setToFirstLabelInRoot();

            void setToLastLabelInRoot();

            void moveToLeftMostKey();

            void moveToRightMostKey();

            void operator++(int);

            void operator--(int);

            std::vector<position_t> getPosition() {
                return pos_in_trie_;
            }

        private:
            void append(const position_t pos);

            void append(const label_t label, const position_t pos);

            void set(const level_t level, const position_t pos);

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
        LoudsSparse() {};

        LoudsSparse(const SuRFBuilder *builder);

        ~LoudsSparse() {}

        // point query: trie walk starts at node "in_node_num" instead of root
        // in_node_num is provided by louds-dense's lookupKey function
        std::optional<uint64_t> lookupKey(const std::vector<label_t> &key, const position_t in_node_num) const;

        std::optional<uint64_t> lookupKey(const std::string &key, const position_t in_node_num) const;

        // return value indicates potential false positive
        bool moveToKeyGreaterThan(const std::vector<label_t> &key,
                                  const bool inclusive, LoudsSparse::Iter &iter) const;

        bool moveToKeyGreaterThan(const std::string &key,
                                  const bool inclusive, LoudsSparse::Iter &iter) const;

        level_t getHeight() const { return height_; };

        level_t getStartLevel() const { return start_level_; };

        uint64_t serializedSize() const;

        uint64_t getMemoryUsage() const;

        uint64_t getValue(level_t level, position_t pos) const {
            assert(values_.size() > level);
            int diff = 0;
            for (int i=start_level_;i<level; i++) {
                diff += values_[i].size();
            }
            pos -= diff;
            assert(values_[level].size() > pos);
            return values_[level][pos];
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
        position_t getChildNodeNum(const position_t pos) const;

        position_t getFirstLabelPos(const position_t node_num) const;

        position_t getLastLabelPos(const position_t node_num) const;

        position_t getSuffixPos(const position_t pos) const;

        position_t nodeSize(const position_t pos) const;

        bool isEndofNode(const position_t pos) const;

        void moveToLeftInNextSubtrie(position_t pos, const position_t node_size,
                                     const label_t label, LoudsSparse::Iter &iter) const;

        // return value indicates potential false positive
        bool compareSuffixGreaterThan(const position_t pos, const std::vector<label_t> &key,
                                      const level_t level, const bool inclusive,
                                      LoudsSparse::Iter &iter) const;

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

        std::vector<std::vector<uint64_t>> values_;
    };

} // namespace surf

#endif // LOUDSSPARSE_H_
