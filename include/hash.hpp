#ifndef HASH_H_
#define HASH_H_

#include <string>

namespace surf {

//******************************************************
//HASH FUNCTION FROM LEVELDB
//******************************************************
    inline uint32_t DecodeFixed32(const std::vector<label_t>& data,size_t position) {
        uint32_t result;
        memcpy(((char *)&result), &data[position], sizeof(result));
        return result;
    }

    inline uint32_t Hash(const std::vector<label_t>& data, uint32_t seed) {
        // Similar to murmur hash
        const uint32_t m = 0xc6a4a793;
        const uint32_t r = 24;
        uint32_t h = seed ^ (data.size() * m);

        int processedBytesCount = 0;
        // Pick up four bytes at a time
        while (processedBytesCount + 4 <= data.size()) {
            uint32_t w = DecodeFixed32(data,processedBytesCount);
            processedBytesCount += 4;
            h += w;
            h *= m;
            h ^= (h >> 16);
        }

        // Pick up remaining bytes
        switch (processedBytesCount - data.size()) {
            case 3:
                h += static_cast<unsigned char>(data[processedBytesCount + 2]) << 16;
            case 2:
                h += static_cast<unsigned char>(data[processedBytesCount + 1]) << 8;
            case 1:
                h += static_cast<unsigned char>(data[processedBytesCount + 0]);
                h *= m;
                h ^= (h >> r);
                break;
        }
        return h;
    }

    inline word_t suffixHash(const std::vector<label_t> &key) {
        return Hash(key, 0xbc9f1d34);
    }

} // namespace surf

#endif // HASH_H_

