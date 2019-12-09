#include <iostream>
#include <vector>

#include "include/surf.hpp"

using namespace surf;

int main() {
    std::vector<std::string> keys = {
	"f",
	"far",
	"fast",
	"s",
	"top",
	"toy",
	"trie",
    };

    std::cout << "Build Index with Keys: { 40, 43, 11048, 2621483 }" << "\n";
    std::vector<uint32_t> intKeys = { 40, 43, 11048, 2621483 };

    // basic surf
    SuRF* surfIntegerIndex = new SuRF(intKeys);
    uint32_t integerKey = 40;
    std::cout << "IntegerLookup: " << (surfIntegerIndex->lookupKey(integerKey) ? "Found key " : "Not found key ") << integerKey << "!\n";
    integerKey = 43;
    std::cout << "IntegerLookup: " << (surfIntegerIndex->lookupKey(integerKey) ? "Found key " : "Not found key ") << integerKey << "!\n";
    integerKey = 11048;
    std::cout << "IntegerLookup: " << (surfIntegerIndex->lookupKey(integerKey) ? "Found key " : "Not found key ") << integerKey << "!\n";
    integerKey = 2621483;
    std::cout << "IntegerLookup: " << (surfIntegerIndex->lookupKey(integerKey) ? "Found key " : "Not found key ") << integerKey << "!\n";
    integerKey = 41;
    std::cout << "IntegerLookup: " << (surfIntegerIndex->lookupKey(integerKey) ? "Found key " : "Not found key ") << integerKey << "!\n";

    uint32_t leftIntegerKey = 39;
    uint32_t rightIntegerKey = 78;
    bool left_included = true;
    bool right_included = true;
    std::cout << "IntegerLookupRange: " << (surfIntegerIndex->lookupRange(leftIntegerKey, true, rightIntegerKey, true) ? "Found key " : "Not found key in range ") << (left_included ? "(" : ")") << leftIntegerKey << "," << rightIntegerKey << (right_included ? ")" : "(") << "!\n";
    leftIntegerKey = 13;
    rightIntegerKey = 38;
    left_included = false;
    right_included = true;
    std::cout << "IntegerLookupRange: " << (surfIntegerIndex->lookupRange(leftIntegerKey, true, rightIntegerKey, true) ? "Found key " : "Not found key in range ") << (left_included ? "(" : ")") << leftIntegerKey << "," << rightIntegerKey << (right_included ? ")" : "(") << "!\n";

    SuRF* surf = new SuRF(keys);

    // use default dense-to-sparse ratio; specify suffix type and length
    SuRF* surf_hash = new SuRF(keys, surf::kHash, 8, 0);
    SuRF* surf_real = new SuRF(keys, surf::kReal, 0, 8);

    // customize dense-to-sparse ratio; specify suffix type and length
    SuRF* surf_mixed = new SuRF(keys, true, 16,  surf::kMixed, 4, 4);

    //----------------------------------------
    // point queries
    //----------------------------------------
    std::cout << "Point Query Example: fase" << std::endl;
    
    std::string key = "fase";
    
    if (surf->lookupKey(key))
	std::cout << "False Positive: "<< key << " found in basic SuRF" << std::endl;
    else
	std::cout << "Correct: " << key << " NOT found in basic SuRF" << std::endl;

    if (surf_hash->lookupKey(key))
	std::cout << "False Positive: " << key << " found in SuRF hash" << std::endl;
    else
	std::cout << "Correct: " << key << " NOT found in SuRF hash" << std::endl;

    if (surf_real->lookupKey(key))
	std::cout << "False Positive: " << key << " found in SuRF real" << std::endl;
    else
	std::cout << "Correct: " << key << " NOT found in SuRF real" << std::endl;

    if (surf_mixed->lookupKey(key))
	std::cout << "False Positive: " << key << " found in SuRF mixed" << std::endl;
    else
	std::cout << "Correct: " << key << " NOT found in SuRF mixed" << std::endl;

    //----------------------------------------
    // range queries
    //----------------------------------------
    std::cout << "\nRange Query Example: [fare, fase)" << std::endl;
    
    std::string left_key = "fare";
    std::string right_key = "fase";

    if (surf->lookupRange(left_key, true, right_key, false))
	std::cout << "False Positive: There exist key(s) within range ["
		  << left_key << ", " << right_key << ") " << "according to basic SuRF" << std::endl;
    else
	std::cout << "Correct: No key exists within range ["
		  << left_key << ", " << right_key << ") " << "according to basic SuRF" << std::endl;

    if (surf_hash->lookupRange(left_key, true, right_key, false))
	std::cout << "False Positive: There exist key(s) within range ["
		  << left_key << ", " << right_key << ") " << "according to SuRF hash" << std::endl;
    else
	std::cout << "Correct: No key exists within range ["
		  << left_key << ", " << right_key << ") " << "according to SuRF hash" << std::endl;

    if (surf_real->lookupRange(left_key, true, right_key, false))
	std::cout << "False Positive: There exist key(s) within range ["
		  << left_key << ", " << right_key << ") " << "according to SuRF real" << std::endl;
    else
	std::cout << "Correct: No key exists within range ["
		  << left_key << ", " << right_key << ") " << "according to SuRF real" << std::endl;

    if (surf_mixed->lookupRange(left_key, true, right_key, false))
	std::cout << "False Positive: There exist key(s) within range ["
		  << left_key << ", " << right_key << ") " << "according to SuRF mixed" << std::endl;
    else
	std::cout << "Correct: No key exists within range ["
		  << left_key << ", " << right_key << ") " << "according to SuRF mixed" << std::endl;

    return 0;
}
