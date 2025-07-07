#pragma once

#include <string>

int GenRandomInt(int min, int max);

uint64_t StrHashUint64(const std::string& str);

std::string StrToInt64(const std::string& str, int64_t& result);
