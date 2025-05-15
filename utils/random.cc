#include "utils/random.h"
#include <random>


int GenRandomInt(int min, int max) {
    static std::random_device rd;  // 获取随机种子
    static std::mt19937 gen(rd()); // 初始化Mersenne Twister随机数生成器
    std::uniform_int_distribution<> distrib(min, max); // 均匀分布
    return distrib(gen);
};


