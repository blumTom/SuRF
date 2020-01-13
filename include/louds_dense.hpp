#ifndef LOUDSDENSE_H_
#define LOUDSDENSE_H_

#include <string>
#include <algorithm>

#include "config.hpp"
#include "rank.hpp"
#include "suffix.hpp"
#include "surf_builder.hpp"

namespace surf {

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

            void clear();

            bool isValid() const { return is_valid_; };

            bool isSearchComplete() const { return is_search_complete_; };

            bool isMoveLeftComplete() const { return is_move_left_complete_; };

            bool isMoveRightComplete() const { return is_move_right_complete_; };

            bool isComplete() const {
                return (is_search_complete_ &&
                        (is_move_left_complete_ && is_move_right_complete_));
            }

            int compare(const std::vector<label_t> &key) const;

            std::vector<label_t> getKey() const;

            int getSuffix(word_t *suffix) const;

            uint64_t getValue() const;

            std::vector<label_t> getKeyWithSuffix(unsigned *bitlen) const;

            position_t getSendOutNodeNum() const { return send_out_node_num_; };

            void setToFirstLabelInRoot();

            void setToLastLabelInRoot();

            void moveToLeftMostKey();

            void moveToRightMostKey();

            void operator++(int);

            void operator--(int);

            std::vector<position_t> getPositions() {
                return pos_in_trie_;
            }

        private:
            inline void append(position_t pos);

            inline void set(level_t level, position_t pos);

            inline void setSendOutNodeNum(position_t node_num) { send_out_node_num_ = node_num; };

            inline void setFlags(const bool is_valid, const bool is_search_complete,
                                 const bool is_move_left_complete,
                                 const bool is_move_right_complete);

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
        LoudsDense() {};

        LoudsDense(const SuRFBuilder *builder);

        ~LoudsDense() {}

        // Returns whether key exists in the trie so far
        // out_node_num == 0 means search terminates in louds-dense.
        std::optional<uint64_t> lookupKey(const std::vector<label_t> &key, position_t &out_node_num) const;

        std::optional<uint64_t> lookupKey(const std::string &key, position_t &out_node_num) const;

        // return value indicates potential false positive
        bool moveToKeyGreaterThan(const std::vector<label_t> &key,
                                  const bool inclusive, LoudsDense::Iter &iter) const;

        bool moveToKeyGreaterThan(const std::string &key,
                                  const bool inclusive, LoudsDense::Iter &iter) const;

        uint64_t getHeight() const { return height_; };

        uint64_t serializedSize() const;

        uint64_t getMemoryUsage() const;

        uint64_t getValue(level_t level, position_t pos) const {
            assert(values_.size() > level);
            int diff = 0;
            for (int i=0;i<level; i++) {
                diff += values_[i].size();
            }
            pos -= diff;
            assert(values_[level].size() > pos);
            return values_[level][pos];
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
        position_t getChildNodeNum(const position_t pos) const;

        position_t getSuffixPos(const position_t pos, const bool is_prefix_key) const;

        position_t getNextPos(const position_t pos) const;

        position_t getPrevPos(const position_t pos, bool *is_out_of_bound) const;

        bool compareSuffixGreaterThan(const position_t pos, const std::vector<label_t> &key,
                                      const level_t level, const bool inclusive,
                                      LoudsDense::Iter &iter) const;

    private:
        static const position_t kNodeFanout = 256;
        static const position_t kRankBasicBlockSize = 512;

        level_t height_;

        BitvectorRank *label_bitmaps_;
        BitvectorRank *child_indicator_bitmaps_;
        BitvectorRank *prefixkey_indicator_bits_; //1 bit per internal node
        BitvectorSuffix *suffixes_;

        std::vector<std::vector<uint64_t>> values_;
    };

} //namespace surf

#endif // LOUDSDENSE_H_
