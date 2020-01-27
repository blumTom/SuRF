#ifndef SURF_VALUE_HPP
#define SURF_VALUE_HPP

#include "bitvector.hpp"

#include <assert.h>

#include <vector>

#include "config.hpp"
#include "hash.hpp"

namespace surf {

    template <typename ValueType>
    class ValueVector {
    public:
        ValueVector() : num_values_(0), values_(nullptr) {};

        ValueVector(const std::vector<std::vector<ValueType>> &values_per_level,
                    const level_t start_level = 0,
                    level_t end_level = 0) {
            if (end_level == 0)
                end_level = values_per_level.size();

            num_values_ = 0;
            for (level_t level = start_level; level < end_level; level++)
                num_values_ += values_per_level[level].size();

            values_ = new ValueType[num_values_];

            position_t pos = 0;
            for (level_t level = start_level; level < end_level; level++) {
                for (position_t idx = 0; idx < values_per_level[level].size(); idx++) {
                    values_[pos] = values_per_level[level][idx];
                    pos++;
                }
            }
        }

        ~ValueVector() {}

        position_t getNumValues() const {
            return num_values_;
        }

        position_t serializedSize() const {
            position_t size = sizeof(num_values_) + num_values_ * sizeof(ValueType);
            sizeAlign(size);
            return size;
        }

        position_t size() const {
            return (sizeof(ValueVector) + num_values_ * sizeof(ValueType));
        }

        ValueType read(const position_t pos) const {
            return values_[pos];
        }

        ValueType operator[](const position_t pos) const {
            return values_[pos];
        }

        void insert(position_t pos, ValueType newvalue) {
            ValueType *newvalues_ = (ValueType*) malloc(sizeof(ValueType) * (num_values_ + 1));
            for (int i=0; i<pos; i++) {
                newvalues_[i] = values_[i];
            }
            newvalues_[pos] = newvalue;
            for (int i=pos; i<num_values_; i++) {
                newvalues_[i+1] = values_[i];
            }
            delete values_;
            values_ = newvalues_;
            num_values_++;
        }

        void remove(position_t pos) {
            for (int i=pos; i<num_values_ - 1; i++) {
                values_[i] = values_[i+1];
            }
            num_values_--;
        }

        void serialize(char *&dst) const {
            memcpy(dst, &num_values_, sizeof(num_values_));
            dst += sizeof(num_values_);
            memcpy(dst, values_, num_values_);
            dst += num_values_;
            align(dst);
        }

        static ValueVector *deSerialize(char *&src) {
            ValueVector *lv = new ValueVector();
            memcpy(&(lv->num_values_), src, sizeof(lv->num_values_));
            src += sizeof(lv->num_values_);
            lv->values_ = const_cast<ValueType *>(reinterpret_cast<const ValueType *>(src));
            src += lv->num_values_;
            align(src);
            return lv;
        }

        void destroy() {
            delete[] values_;
        }

    private:
        position_t num_values_;
        ValueType *values_;
    };

} // namespace surf

#endif //SURF_VALUE_HPP
