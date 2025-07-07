#include "utils/numeric_util.h"
#include <random>
#include <string>
#include <functional>
#include <cmath>
#include <cstdint>  // for int64_t


int GenRandomInt(int min, int max) {
    static std::random_device rd;  // 获取随机种子
    static std::mt19937 gen(rd()); // 初始化Mersenne Twister随机数生成器
    std::uniform_int_distribution<> distrib(min, max); // 均匀分布
    return distrib(gen);
};



uint64_t StrHashUint64(const std::string& str) {
    std::hash<std::string> hasher;
    int64_t hash_value = hasher(str);
    // std::cout << "Hash value: " << hash_value << std::endl;
    return std::abs(hash_value);
}


std::string StrToInt64(const std::string& str, int64_t& result) {
    try {
        size_t pos;
        result = std::stoll(str, &pos);
        // 检查是否整个字符串都被成功转换了（无非法字符残留）
        if (pos != str.length()) {
            return "Invalid input: not a valid int64_t string";
        }
        return "";
    } catch (const std::invalid_argument& e) {
        // 字符串为空或不是数字
        return "Invalid input: not a valid int64_t string";
    } catch (const std::out_of_range& e) {
        // 超出了 int64_t 表示范围
        return "Out of range: int64_t overflow";
    }
}