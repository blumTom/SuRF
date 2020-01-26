#include "bitvector.hpp"

namespace surf {

    bool Bitvector::readBit(const position_t pos) const {
        assert(pos < num_bits_);
        position_t word_id = pos / kWordSize;
        position_t offset = pos & (kWordSize - 1);
        return bits_[word_id] & (kMsbMask >> offset);
    }

    void Bitvector::setBit(const position_t pos) {
        if (pos >= num_bits_) return;

        position_t word_id = pos / kWordSize;
        position_t offset = pos & (kWordSize - 1);

        bits_[word_id] |= (0x8000000000000000 >> offset);
    }

    void Bitvector::unsetBit(const position_t pos) {
        if (pos >= num_bits_) return;

        position_t word_id = pos / kWordSize;
        position_t offset = pos & (kWordSize - 1);

        bits_[word_id] &= 0xFFFFFFFFFFFFFFFF ^ (0x8000000000000000 >> offset);
    }

    void Bitvector::insert(const position_t pos, bool isSet) {
        if (pos > num_bits_) return;

        position_t bit_pos = pos;
        position_t word_id = bit_pos / kWordSize;
        position_t offset = bit_pos & (kWordSize - 1);

        position_t freeBits = numWords() * sizeof(word_t) * 8 - num_bits_;

        if (freeBits == 0) {
            word_t* newbits_ = (word_t*) malloc(numWords() + 1);
            for (int i=0; i<numWords(); i++) {
                newbits_[i] = bits_[i];
            }
            newbits_[numWords()] = 0;
            delete bits_;
            bits_ = newbits_;
        }

        word_t bitsToCopy = (bits_[word_id] << (64 - 1));
        bits_[word_id] >>= 1;
        word_t bitsAfterNewSuffix = bits_[word_id] & (0xFFFFFFFFFFFFFFFF >> (offset + 1));
        bits_[word_id] >>= 64 - offset - 1;
        bits_[word_id] <<= 1;
        if (isSet) bits_[word_id] += 1;
        bits_[word_id] <<= 64 - offset - 1;
        bits_[word_id] += bitsAfterNewSuffix;

        num_bits_ += (position_t) 1;

        for (int i=word_id + 1; i<numWords(); i++) {
            word_t nextBitsToCopy = (bits_[i] << (64 - 1));
            bits_[i] >>= 1;
            bits_[i] = bits_[i] | bitsToCopy;
            bitsToCopy = nextBitsToCopy;
        }
    }

    void Bitvector::insertWords(const position_t pos, const int count) {
        if (pos > num_bits_) return;

        position_t bit_pos = pos;
        position_t word_id = bit_pos / kWordSize;

        int bytecount = count * sizeof(word_t);

        word_t* newbits_ = new word_t[numWords() + count];
        for (int i=0; i<word_id; i++) {
            newbits_[i] = bits_[i];
        }
        for (int i=0; i<word_id; i++) {
            newbits_[word_id + i] = 0;
        }
        for (int i=word_id; i<numWords(); i++) {
            newbits_[i + count] = bits_[i];
        }
        delete bits_;
        bits_ = newbits_;

        num_bits_ += (position_t) bytecount * 8;
    }

    position_t Bitvector::distanceToNextSetBit(const position_t pos) const {
        assert(pos < num_bits_);
        position_t distance = 1;

        position_t word_id = (pos + 1) / kWordSize;
        position_t offset = (pos + 1) % kWordSize;

        //first word left-over bits
        word_t test_bits = bits_[word_id] << offset;
        if (test_bits > 0) {
            return (distance + __builtin_clzll(test_bits));
        } else {
            if (word_id == numWords() - 1)
                return (num_bits_ - pos);
            distance += (kWordSize - offset);
        }

        while (word_id < numWords() - 1) {
            word_id++;
            test_bits = bits_[word_id];
            if (test_bits > 0)
                return (distance + __builtin_clzll(test_bits));
            distance += kWordSize;
        }
        return distance;
    }

    position_t Bitvector::distanceToPrevSetBit(const position_t pos) const {
        assert(pos <= num_bits_);
        if (pos == 0) return 0;
        position_t distance = 1;

        position_t word_id = (pos - 1) / kWordSize;
        position_t offset = (pos - 1) % kWordSize;

        //first word left-over bits
        word_t test_bits = bits_[word_id] >> (kWordSize - 1 - offset);
        if (test_bits > 0) {
            return (distance + __builtin_ctzll(test_bits));
        } else {
            //if (word_id == 0)
            //return (offset + 1);
            distance += (offset + 1);
        }

        while (word_id > 0) {
            word_id--;
            test_bits = bits_[word_id];
            if (test_bits > 0)
                return (distance + __builtin_ctzll(test_bits));
            distance += kWordSize;
        }
        return distance;
    }

    position_t Bitvector::totalNumBits(const std::vector<position_t> &num_bits_per_level,
                                       const level_t start_level,
                                       const level_t end_level/* non-inclusive */) {
        position_t num_bits = 0;
        for (level_t level = start_level; level < end_level; level++)
            num_bits += num_bits_per_level[level];
        return num_bits;
    }

    void Bitvector::concatenateBitvectors(const std::vector<std::vector<word_t> > &bitvector_per_level,
                                          const std::vector<position_t> &num_bits_per_level,
                                          const level_t start_level,
                                          const level_t end_level/* non-inclusive */) {
        position_t bit_shift = 0;
        position_t word_id = 0;
        for (level_t level = start_level; level < end_level; level++) {
            if (num_bits_per_level[level] == 0) continue;
            position_t num_complete_words = num_bits_per_level[level] / kWordSize;
            for (position_t word = 0; word < num_complete_words; word++) {
                bits_[word_id] |= (bitvector_per_level[level][word] >> bit_shift);
                word_id++;
                if (bit_shift > 0)
                    bits_[word_id] |= (bitvector_per_level[level][word] << (kWordSize - bit_shift));
            }

            word_t bits_remain = num_bits_per_level[level] - num_complete_words * kWordSize;
            if (bits_remain > 0) {
                word_t last_word = bitvector_per_level[level][num_complete_words];
                bits_[word_id] |= (last_word >> bit_shift);
                if (bit_shift + bits_remain < kWordSize) {
                    bit_shift += bits_remain;
                } else {
                    word_id++;
                    bits_[word_id] |= (last_word << (kWordSize - bit_shift));
                    bit_shift = bit_shift + bits_remain - kWordSize;
                }
            }
        }
    }

}