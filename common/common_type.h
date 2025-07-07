
#pragma once

#include <string>

#define _ID "_id"

enum class DType : int8_t {
    UNDEFINE = -1,
    INT = 0,
    LONG = 1,
    FLOAT = 2,
    DOUBLE = 3,
    STRING = 4,
    STRINGARRAY = 5,
    VECTOR = 6
};


extern const std::string UNDEFINE_SDTYPE;
extern const std::string INT_SDTYPE;
extern const std::string LONG_SDTYPE;
extern const std::string FLOAT_SDTYPE;
extern const std::string DOUBLE_SDTYPE;
extern const std::string STRING_SDTYPE;
extern const std::string STRINGARRAY_SDTYPE;
extern const std::string VECTOR_SDTYPE;

const std::string& DTypeToSDType(DType t);

DType SDTypeToDType(const std::string& sdtype);

bool IsIdDType(DType dtype);


