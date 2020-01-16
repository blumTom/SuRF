#ifndef FILTER_SURF_H_
#define FILTER_SURF_H_

#include <string>
#include <vector>

#include "surf.hpp"

namespace bench {

class FilterSuRF : public Filter {
public:
    // Requires that keys are sorted
    FilterSuRF(const std::vector<std::string>& keys,
	       const surf::SuffixType suffix_type,
               const uint32_t hash_suffix_len, const uint32_t real_suffix_len) {
	// uses default sparse-dense size ratio
	std::vector<std::pair<std::string,uint64_t>> keyValues;
	for (uint64_t i=0; i<keys.size(); i++) {
	    keyValues.emplace_back(std::make_pair(keys[i],i));
	}
	filter_ = new surf::SuRF<std::string,uint64_t>(keyValues, surf::kIncludeDense, surf::kSparseDenseRatio,
				 suffix_type, hash_suffix_len, real_suffix_len,surf::stringToByteVector);
    }

    ~FilterSuRF() {
	filter_->destroy();
	delete filter_;
    }

    bool lookup(const std::string& key) {
	return filter_->lookupKey(key).has_value();
    }

    bool lookupRange(const std::string& left_key, const std::string& right_key) {
	//return filter_->lookupRange(left_key, false, right_key, false);
	return filter_->lookupRange(left_key, true, right_key, true).size() > 0;
    }

    uint64_t getMemoryUsage() {
	return filter_->getMemoryUsage();
    }

private:
    surf::SuRF<std::string,uint64_t>* filter_;
};

} // namespace bench

#endif // FILTER_SURF_H
