#include "suffix.hpp"

namespace surf {

    word_t BitvectorSuffix::read(const position_t idx) const {
        if (type_ == kNone)
            return 0;

        level_t suffix_len = getSuffixLen();
        if (idx * suffix_len >= num_bits_)
            return 0;

        position_t bit_pos = idx * suffix_len;
        position_t word_id = bit_pos / kWordSize;
        position_t offset = bit_pos & (kWordSize - 1);
        word_t ret_word = (bits_[word_id] << offset) >> (kWordSize - suffix_len);
        if (offset + suffix_len > kWordSize)
            ret_word += (bits_[word_id + 1] >> (kWordSize - offset - suffix_len));
        return ret_word;
    }

    word_t BitvectorSuffix::readReal(const position_t idx) const {
        return extractRealSuffix(read(idx), real_suffix_len_);
    }

    bool BitvectorSuffix::checkEquality(const position_t idx,
                                        const std::vector<label_t> &key, const level_t level) const {
        if (type_ == kNone)
            return true;
        if (idx * getSuffixLen() >= num_bits_)
            return false;

        word_t stored_suffix = read(idx);
        if (type_ == kReal) {
            // if no suffix info for the stored key
            if (stored_suffix == 0)
                return true;
            // if the querying key is shorter than the stored key
            if (key.size() < level || ((key.size() - level) * 8) < real_suffix_len_)
                return false;
        }
        word_t querying_suffix
                = constructSuffix(type_, key, hash_suffix_len_, level, real_suffix_len_);
        return (stored_suffix == querying_suffix);
    }

    void BitvectorSuffix::insertSuffix(const position_t idx, word_t &suffix) {
        if (type_ == kNone) return;

        level_t suffix_len = getSuffixLen();
        if (idx * suffix_len > num_bits_) return;

        position_t bit_pos = idx * suffix_len;
        position_t word_id = bit_pos / kWordSize;
        position_t offset = bit_pos & (kWordSize - 1);

        position_t freeBits = numWords() * sizeof(word_t) * 8 - num_bits_;

        if (suffix_len > freeBits) {
            word_t* newbits_ = (word_t*) malloc(numWords() + 1);
            for (int i=0; i<numWords(); i++) {
                newbits_[i] = bits_[i];
            }
            newbits_[numWords()] = 0;
            delete bits_;
            bits_ = newbits_;
        }

        word_t bitsToCopy = (bits_[word_id] << (64 - suffix_len));
        bits_[word_id] >>= suffix_len;
        word_t bitsAfterNewSuffix = bits_[word_id] & (0xFFFFFFFFFFFFFFFF >> (offset + suffix_len));
        bits_[word_id] >>= 64 - offset - suffix_len;
        bits_[word_id] <<= suffix_len;
        bits_[word_id] += suffix;
        bits_[word_id] <<= 64 - offset - suffix_len;
        bits_[word_id] += bitsAfterNewSuffix;

        num_bits_ += (position_t) suffix_len;

        for (int i=word_id + 1; i<numWords(); i++) {
            word_t nextBitsToCopy = (bits_[i] << (64 - suffix_len));
            bits_[i] >>= suffix_len;
            bits_[i] = bits_[i] | bitsToCopy;
            bitsToCopy = nextBitsToCopy;
        }
    }

    void BitvectorSuffix::removeSuffix(const position_t idx) {
        if (type_ == kNone) return;

        level_t suffix_len = getSuffixLen();
        if (idx * suffix_len >= num_bits_) return;

        position_t bit_pos = idx * suffix_len;
        position_t word_id = bit_pos / kWordSize;
        position_t offset = bit_pos & (kWordSize - 1);

        word_t bitsToCopy = 0;
        for (int i=numWords() - 1; i>word_id; i--) {
            word_t nextBitsToCopy = (bits_[i] >> (64 - suffix_len));
            bits_[i] <<= suffix_len;
            bits_[i] += bitsToCopy;
            bitsToCopy = nextBitsToCopy;
        }

        word_t bitsAfterNewSuffix = bits_[word_id] & (0xFFFFFFFFFFFFFFFF >> (suffix_len + offset));
        bits_[word_id] >>= 64 - offset;
        bits_[word_id] <<= 64 - offset - suffix_len;
        bits_[word_id] += bitsAfterNewSuffix;
        bits_[word_id] <<= suffix_len;
        bits_[word_id] += bitsToCopy;

        num_bits_ -= (position_t) suffix_len;
    }

// If no real suffix is stored for the key, compare returns 0.
// int BitvectorSuffix::compare(const position_t idx,
// 			     const std::string& key, const level_t level) const {
//     if ((type_ == kNone) || (type_ == kHash) || (idx * getSuffixLen() >= num_bits_))
// 	return 0;
//     word_t stored_suffix = read(idx);
//     word_t querying_suffix = constructRealSuffix(key, level, real_suffix_len_);
//     if (type_ == kMixed)
//         stored_suffix = extractRealSuffix(stored_suffix, real_suffix_len_);

//     if (stored_suffix == 0)
// 	return 0;
//     if (stored_suffix < querying_suffix)
// 	return -1;
//     else if (stored_suffix == querying_suffix)
// 	return 0;
//     else
// 	return 1;
// }

    int BitvectorSuffix::compare(const position_t idx,
                                 const std::vector<label_t> &key, const level_t level) const {
        if ((idx * getSuffixLen() >= num_bits_) || (type_ == kNone) || (type_ == kHash))
            return kCouldBePositive;

        word_t stored_suffix = read(idx);
        word_t querying_suffix = constructRealSuffix(key, level, real_suffix_len_);
        if (type_ == kMixed)
            stored_suffix = extractRealSuffix(stored_suffix, real_suffix_len_);

        if ((stored_suffix == 0) && (querying_suffix == 0))
            return kCouldBePositive;
        else if ((stored_suffix == 0) || (stored_suffix < querying_suffix))
            return -1;
        else if (stored_suffix == querying_suffix)
            return kCouldBePositive;
        else
            return 1;
    }

}