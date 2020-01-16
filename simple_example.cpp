#include <iostream>
#include <vector>

#include "include/surf.hpp"

using namespace surf;

int main() {
    std::vector<std::pair<std::string,uint64_t>> keys = {
            std::make_pair("f",1),
            std::make_pair("far",2),
            std::make_pair("fast",3),
            std::make_pair("s",4),
            std::make_pair("top",5),
            std::make_pair("toy",6),
            std::make_pair("trie",7),
    };

    /*
     673786411 = 0x28292A2B
     2621483   = 0x0028002B
     11048     = 0x00002B28
     10240     = 0x00002800
     43        = 0x0000002B
     40        = 0x00000028
     */
    //std::cout << "Build Index with Keys: { 673786411, 2621483, 11048, 43, 40 }" << "\n";
    /*std::vector<uint32_t> intKeys = { 16843265, 16843009, 16843266, 16843010, 16843267, 16843011 };

    // basic surf
    SuRF *surfIntegerIndex = new SuRF(intKeys);
    uint32_t integerKey = 16843009;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";
    integerKey = 16843010;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";
    integerKey = 16843011;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";
    integerKey = 16843012;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";
    integerKey = 16843265;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";
    integerKey = 16843266;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";
    integerKey = 16843267;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";
    integerKey = 16843268;
    std::cout << "Value: " << surfIntegerIndex->lookupKey(integerKey).has_value() << "\n";

    uint32_t leftIntegerKey = 39;
    uint32_t rightIntegerKey = 78;
    bool left_included = true;
    bool right_included = true;
    std::cout << "IntegerLookupRange: "
              << (surfIntegerIndex->lookupRange(leftIntegerKey, true, rightIntegerKey, true) ? "Found key "
                                                                                             : "Not found key in range ")
              << (left_included ? "(" : ")") << leftIntegerKey << "," << rightIntegerKey << (right_included ? ")" : "(")
              << "!\n";
    leftIntegerKey = 13;
    rightIntegerKey = 38;
    left_included = false;
    right_included = true;
    std::cout << "IntegerLookupRange: "
              << (surfIntegerIndex->lookupRange(leftIntegerKey, true, rightIntegerKey, true) ? "Found key "
                                                                                             : "Not found key in range ")
              << (left_included ? "(" : ")") << leftIntegerKey << "," << rightIntegerKey << (right_included ? ")" : "(")
              << "!\n";
    std::cout << "\n";*/

    std::function<std::vector<label_t>(const std::string&)> keyDerivator = [](const std::string& key) { return stringToByteVector(key); };

    SuRF<std::string,uint64_t> *surf = new SuRF<std::string,uint64_t>(keys,keyDerivator);
    std::cout << "Memory Usage: " << surf->getMemoryUsage() << " Bytes\n";

    // use default dense-to-sparse ratio; specify suffix type and length
    SuRF<std::string,uint64_t> *surf_hash = new SuRF<std::string,uint64_t>(keys, surf::kHash, 8, 0,keyDerivator);
    SuRF<std::string,uint64_t> *surf_real = new SuRF<std::string,uint64_t>(keys, surf::kReal, 0, 8,keyDerivator);

    // customize dense-to-sparse ratio; specify suffix type and length
    SuRF<std::string,uint64_t> *surf_mixed = new SuRF<std::string,uint64_t>(keys, true, 16, surf::kMixed, 4, 4,keyDerivator);

    //----------------------------------------
    // point queries
    //----------------------------------------
    std::cout << "Point Query Example: fase" << std::endl;

    std::string key = "fase";

    std::optional<uint64_t> result = surf->lookupKey(key);
    if (result.has_value())
        std::cout << "False Positive: " << key << " found in basic SuRF: " << result.value() << std::endl;
    else
        std::cout << "Correct: " << key << " NOT found in basic SuRF" << std::endl;

    result = surf_hash->lookupKey(key);
    if (result.has_value())
        std::cout << "False Positive: " << key << " found in SuRF hash: " << result.value() << std::endl;
    else
        std::cout << "Correct: " << key << " NOT found in SuRF hash" << std::endl;

    result = surf_real->lookupKey(key);
    if (result.has_value())
        std::cout << "False Positive: " << key << " found in SuRF real: " << result.value() << std::endl;
    else
        std::cout << "Correct: " << key << " NOT found in SuRF real" << std::endl;

    result = surf_mixed->lookupKey(key);
    if (result.has_value())
        std::cout << "False Positive: " << key << " found in SuRF mixed: " << result.value() << std::endl;
    else
        std::cout << "Correct: " << key << " NOT found in SuRF mixed" << std::endl;

    //----------------------------------------
    // range queries
    //----------------------------------------
    std::cout << "\nRange Query Example: [fare, fase)" << std::endl;

    std::string left_key = "fare";
    std::string right_key = "fase";

    if (surf->lookupRange(left_key, true, right_key, false).size() > 0)
        std::cout << "False Positive: There exist key(s) within range ["
                  << left_key << ", " << right_key << ") " << "according to basic SuRF" << std::endl;
    else
        std::cout << "Correct: No key exists within range ["
                  << left_key << ", " << right_key << ") " << "according to basic SuRF" << std::endl;

    if (surf_hash->lookupRange(left_key, true, right_key, false).size() > 0)
        std::cout << "False Positive: There exist key(s) within range ["
                  << left_key << ", " << right_key << ") " << "according to SuRF hash" << std::endl;
    else
        std::cout << "Correct: No key exists within range ["
                  << left_key << ", " << right_key << ") " << "according to SuRF hash" << std::endl;

    if (surf_real->lookupRange(left_key, true, right_key, false).size() > 0)
        std::cout << "False Positive: There exist key(s) within range ["
                  << left_key << ", " << right_key << ") " << "according to SuRF real" << std::endl;
    else
        std::cout << "Correct: No key exists within range ["
                  << left_key << ", " << right_key << ") " << "according to SuRF real" << std::endl;

    if (surf_mixed->lookupRange(left_key, true, right_key, false).size() > 0)
        std::cout << "False Positive: There exist key(s) within range ["
                  << left_key << ", " << right_key << ") " << "according to SuRF mixed" << std::endl;
    else
        std::cout << "Correct: No key exists within range ["
                  << left_key << ", " << right_key << ") " << "according to SuRF mixed" << std::endl;

    return 0;
}
