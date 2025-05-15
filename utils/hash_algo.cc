#include <string>
#include <functional>
#include <cmath>
#include "utils/hash_algo.h"

uint64_t StrHashUint64(const std::string& str) {
    std::hash<std::string> hasher;
    int64_t hash_value = hasher(str);
    // std::cout << "Hash value: " << hash_value << std::endl;
    return std::abs(hash_value);
}


